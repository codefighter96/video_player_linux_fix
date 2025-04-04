/*
 * Copyright 2020-2024 Toyota Connected North America
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "ecs.h"

// #include <spdlog/spdlog.h>
#include <asio/post.hpp>
#include <chrono>
#include <thread>
#include <core/utils/kvtree.cc> // NOLINT

namespace plugin_filament_view {

template class KVTree<EntityGUID, std::shared_ptr<EntityObject>>;

////////////////////////////////////////////////////////////////////////////
ECSManager* ECSManager::m_poInstance = nullptr;
ECSManager* ECSManager::GetInstance() {
  if (m_poInstance == nullptr) {
    m_poInstance = new ECSManager();
  }

  return m_poInstance;
}

////////////////////////////////////////////////////////////////////////////
ECSManager::~ECSManager() {
  spdlog::debug("ECSManager~");
}

////////////////////////////////////////////////////////////////////////////
ECSManager::ECSManager()
    : io_context_(std::make_unique<asio::io_context>(ASIO_CONCURRENCY_HINT_1)),
      work_(make_work_guard(io_context_->get_executor())),
      strand_(std::make_unique<asio::io_context::strand>(*io_context_)),
      m_eCurrentState(NotInitialized) {
  vSetupThreadingInternals();
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::StartMainLoop() {
  if (m_bIsRunning) {
    return;
  }

  m_bIsRunning = true;
  m_bSpawnedThreadFinished = false;

  // Launch MainLoop in a separate thread
  loopThread_ = std::thread(&ECSManager::MainLoop, this);
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::vSetupThreadingInternals() {
  filament_api_thread_ = std::thread([this] {
    // Save this thread's ID as it runs io_context_->run()
    filament_api_thread_id_ = pthread_self();

    // Optionally set the thread name
    pthread_setname_np(filament_api_thread_id_, "ECSManagerThreadRunner");

    spdlog::debug("ECSManager Filament API thread started: 0x{:x}",
                  filament_api_thread_id_);

    io_context_->run();
  });
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::MainLoop() {
  constexpr std::chrono::milliseconds frameTime(16);  // ~1/60 second

  // Initialize lastFrameTime to the current time
  auto lastFrameTime = std::chrono::steady_clock::now();

  m_eCurrentState = Running;
  while (m_bIsRunning) {
    auto start = std::chrono::steady_clock::now();

    // Calculate the time difference between this frame and the last frame
    std::chrono::duration<float> elapsedTime = start - lastFrameTime;

    if (!isHandlerExecuting.load()) {
      // Use asio::post to schedule work on the main thread (API thread)
      post(*strand_, [elapsedTime = elapsedTime.count(), this] {
        isHandlerExecuting.store(true);
        try {
          vUpdate(
              elapsedTime);  // Pass elapsed time to the main thread
        } catch (...) {
          isHandlerExecuting.store(false);
          throw;  // Rethrow the exception after resetting the flag
        }
        isHandlerExecuting.store(false);
      });
    }

    // Update the time for the next frame
    lastFrameTime = start;

    auto end = std::chrono::steady_clock::now();

    // Sleep for the remaining time in the frame
    if (std::chrono::duration<double> elapsed = end - start;
        elapsed < frameTime) {
      std::this_thread::sleep_for(frameTime - elapsed);
    }
  }
  m_eCurrentState = ShutdownStarted;

  m_bSpawnedThreadFinished = true;
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::StopMainLoop() {
  m_bIsRunning = false;
  if (loopThread_.joinable()) {
    loopThread_.join();
  }

  // Stop the io_context
  io_context_->stop();

  // Reset the work guard
  work_.reset();

  // Join the filament_api_thread_
  if (filament_api_thread_.joinable()) {
    filament_api_thread_.join();
  }
}


//
//  Entity
//


void ECSManager::addEntity(const std::shared_ptr<EntityObject>& entity, const EntityGUID* parent) {
  std::unique_lock lock(_entitiesMutex);

  const EntityGUID id = entity->GetGuid();

  if (_entities.get(id)) {
    spdlog::error(
      "[{}] Entity with GUID {} already exists",
      __FUNCTION__, id
    );
    return;
  }

  _entities.insert(id, entity, parent);
}

void ECSManager::removeEntity(const EntityGUID& id) {
  std::unique_lock lock(_entitiesMutex);

  _entities.remove(id);
}

std::shared_ptr<EntityObject> ECSManager::getEntity(EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[{}] Unable to find "
        "entity with id {}",
        __FUNCTION__,
        id);
    return nullptr;
  }
  return *(node->getValue());
}

void ECSManager::reparentEntity(
  const std::shared_ptr<EntityObject>& entity,
  const EntityGUID& parentGuid) {
try {
  _entities.reparent(entity->GetGuid(), &parentGuid);
} catch (const std::runtime_error& e) {
  spdlog::error("[ECSManager::ReparentEntityObject] {}",
                e.what());
}
}

std::vector<EntityGUID> ECSManager::getEntityChildrenGuids(EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[{}] Unable to find entity with id {}",
        __FUNCTION__,
        id);
    return {};
  }

  std::vector<KVTreeNode<EntityGUID, std::shared_ptr<EntityObject>>*>
      childrenNodes = node->getChildren();

  std::vector<EntityGUID> childrenGuids;
  childrenGuids.reserve(childrenNodes.size());
  for (const auto* childNode : childrenNodes) {
    childrenGuids.push_back(childNode->getKey());
  }

  return childrenGuids;
}

std::vector<std::shared_ptr<EntityObject>> ECSManager::getEntityChildren(EntityGUID id) const {
  std::vector<EntityGUID> childrenGuids = getEntityChildrenGuids(id);

  std::vector<std::shared_ptr<EntityObject>> children;
  children.reserve(childrenGuids.size());
  for (const auto& childGuid : childrenGuids) {
    const auto child = getEntity(childGuid);
    if (child) {
      children.push_back(child);
    }
  }

  return children;
}

std::optional<EntityGUID> ECSManager::getEntityParentGuid(EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[ECSManager::GetEntityParentGuid] Unable to find "
        "entity with id {}",
        id);
    return std::nullopt;
  }

  const auto* parentNode = node->getParent();
  if (!parentNode) {
    return std::nullopt;
  }

  return parentNode->getKey();
}




















////////////////////////////////////////////////////////////////////////////
void ECSManager::vInitSystems() {
  // Note this is currently expected to be called from within
  // an already asio post, Leaving this commented out so you know
  // that you could change up the routine, but if you do
  // it needs to run on the main thread.
  // asio::post(*ECSManager::GetInstance()->GetStrand(), [&] {
  for (const auto& system : m_vecSystems) {
    try {
      spdlog::debug("Initializing system {} (ID {}) at address {}",
                    system->GetTypeName(), system->GetTypeID(), static_cast<void*>(system.get()));
      
      system->vInitSystem(*const_cast<const ECSManager*>(this));

    } catch (const std::exception& e) {
      spdlog::error("Exception caught in vInitSystems: {}", e.what());
    }
  }

  spdlog::info("All systems initialized");
  m_eCurrentState = Initialized;

  //});
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::vAddSystem(std::shared_ptr<ECSystem> system) {
  std::unique_lock lock(vecSystemsMutex);
  spdlog::debug("Adding system at address {}",
                static_cast<void*>(system.get()));
  m_vecSystems.push_back(std::move(system));
}

void ECSManager::vRemoveSystem(const std::shared_ptr<ECSystem>& system) {
  std::unique_lock lock(vecSystemsMutex);
  auto it = std::remove(m_vecSystems.begin(), m_vecSystems.end(), system);
  if (it != m_vecSystems.end()) {
    m_vecSystems.erase(it, m_vecSystems.end());
  }
}

void ECSManager::vRemoveAllSystems() {
  std::unique_lock lock(vecSystemsMutex);
  m_vecSystems.clear();
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::vUpdate(const float deltaTime) {
  // Copy systems under mutex
  std::vector<std::shared_ptr<ECSystem>> systemsCopy;
  {
    std::unique_lock lock(vecSystemsMutex);

    // Copy the systems vector
    systemsCopy = m_vecSystems;
  }  // Mutex is unlocked here

  // Iterate over the copy without holding the mutex
  for (const auto& system : systemsCopy) {
    if (system) {
      system->vProcessMessages();
      system->vUpdate(deltaTime);
    } else {
      spdlog::error("Encountered null system pointer!");
    }
  }
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::DebugPrint() const {
  for (const auto& system : m_vecSystems) {
    spdlog::debug(
        "ECSManager:: DebugPrintProcessing system at address {}, "
        "use_count={}",
        static_cast<void*>(system.get()), system.use_count());
  }
}

////////////////////////////////////////////////////////////////////////////
void ECSManager::vShutdownSystems() {
  post(*this->GetStrand(), [&] {
    // we shutdown in reverse, until we have a 'system dependency tree' type of
    // view, filament system (which is always the first system, needs to be
    // shutdown last as its 'engine' varible is used in destruction for other
    // systems
    for (auto it = m_vecSystems.rbegin(); it != m_vecSystems.rend(); ++it) {
      (*it)->vShutdownSystem();
    }

    m_eCurrentState = Shutdown;
  });
}

}  // namespace plugin_filament_view