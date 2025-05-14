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

#include "transform_system.h"

#include <core/systems/ecs.h>
#include <core/systems/derived/filament_system.h>
#include <spdlog/spdlog.h>


namespace plugin_filament_view {

void TransformSystem::vOnInitSystem() {
  // Get filament
  const auto filamentSystem = ecs->getSystem<FilamentSystem>(__FUNCTION__);
  const auto engine = filamentSystem->getFilamentEngine();

  tm = &(engine->getTransformManager());
}

void TransformSystem::vShutdownSystem() {
  // Shutdown the system
  spdlog::debug("TransformSystem shutdown");
}

void TransformSystem::vProcessMessages() {
  // Process incoming messages
  // spdlog::debug("TransformSystem processing messages");
}

void TransformSystem::vHandleMessage(const ECSMessage& msg) {
  // Handle incoming messages
  // spdlog::debug("TransformSystem handling message");
}

void TransformSystem::DebugPrint() {
  spdlog::debug("TransformSystem DebugPrint");
}

//
// Internal logic 
//

void TransformSystem::updateTransforms() {
  // Update transforms
  // spdlog::debug("TransformSystem updating transforms");
}

void TransformSystem::updateFilamentParentTree() {
  // Update Filament parent tree
  // spdlog::debug("TransformSystem updating Filament parent tree");
}

//
// Utility functions
//

void TransformSystem::applyTransform(
  const EntityGUID entityId,
  const bool forceRecalculate
) {
  // Get the entity and its transform
  const auto entity = ecs->getEntity(entityId);
  if (entity == nullptr) {
    throw std::runtime_error("Entity not found");
  }

  applyTransform(*(entity.get()), forceRecalculate);
}

void TransformSystem::applyTransform(
  const EntityObject& entity,
  const bool forceRecalculate
) {
  const std::shared_ptr<BaseTransform> transform = std::dynamic_pointer_cast<BaseTransform>(
    entity.GetComponent(Component::StaticGetTypeID<BaseTransform>())
  );
  if (transform == nullptr) {
    throw std::runtime_error("Transform component not found");
  }

  // Recalculate the transform only if it's dirty or forced
  if(!forceRecalculate || !transform->IsDirty()) {
    return;
  }

  const filament::math::mat4f localMatrix = calculateTransform(*transform);

  const auto _fInstance = tm->getInstance(entity._fEntity);
  tm->setTransform(_fInstance, localMatrix);

  transform->SetDirty(false);
}

void TransformSystem::reparentEntity(
  const EntityObject& entity,
  const EntityObject& parent
) {
  const auto fEntity = entity._fEntity;
  const auto fInstance = tm->getInstance(fEntity);
  const auto parent_fEntity = parent._fEntity;

  // If a parent ID is provided, set the parent transform
  // if (parent != nullptr) {
    // Get instance of parent
    const auto parent_fInstance = tm->getInstance(parent_fEntity);
    // Get current parent
    const auto currentParent_fEntity = tm->getParent(fInstance);

    // Skip parenting if same as current parent
    if(currentParent_fEntity == parent_fEntity) {
      spdlog::debug("[{}] New parent entity is the same as the current Parent entity ({}), skipping reparenting.",
                    __FUNCTION__, parent_fEntity.getId());
      return;
    }

    // Check if the parent instance is valid
    if(parent_fInstance.isValid()) {
      if(fInstance.asValue() == parent_fInstance.asValue()) {
        throw std::runtime_error(
          fmt::format("[{}] New parent instance is the same as the current instance ({}), skipping parenting.",
                      __FUNCTION__, fInstance.asValue()));
      }
    } else {
      throw std::runtime_error(
        fmt::format("[{}] Parent instance ? of {} is not valid.",
                    __FUNCTION__, fEntity.getId()));
    }


    // Set the parent transform
    try {
      // make sure the parent has a valid transform
      // const auto parentTransform = tm->getTransform(parent_fInstance);
      // assert(parentTransform.isValid());

      tm->setParent(fInstance, parent_fInstance);
    } catch (const std::exception& e) {
      spdlog::error("[{}] Error setting parent: {}", __FUNCTION__, e.what());
    } catch (...) {
      spdlog::error("[{}] Unknown error setting parent", __FUNCTION__);
    }
  // } else {
  //   // TODO: set parent to null/root, how?
  // }
}

}  // namespace plugin_filament_view
