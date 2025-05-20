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

#include <spdlog/spdlog.h>

#include <core/systems/ecs.h>
#include <core/systems/derived/filament_system.h>
#include <core/utils/asserts.h>
#include <core/utils/vectorutils.h>


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
  const auto transforms = ecs->getComponentsOfType<BaseTransform>();
  for (const auto& transform : transforms) {
    applyTransform(*(transform.get()), false);
  }
}

void TransformSystem::updateFilamentParentTree() {
  // Update Filament parent tree
}

//
// Utility functions
//

void TransformSystem::applyTransform(
  const EntityGUID entityId,
  const bool forceRecalculate
) {
  // Get the entity and its transform
  const auto transform = ecs->getComponent<BaseTransform>(entityId);
  if (transform == nullptr) {
    throw std::runtime_error("Entity not found");
  }

  applyTransform(*(transform.get()), forceRecalculate);
}

void TransformSystem::applyTransform(
  BaseTransform& transform,
  const bool forceRecalculate
) {
  // Recalculate the transform only if it's dirty or forced
  if(!forceRecalculate && !transform.IsDirty()) {
    return;
  }

  const EntityGUID entityId = transform.GetOwner()->GetGuid();
  filament::math::mat4f localMatrix = filament::gltfio::composeMatrix(
    transform.local.position,
    transform.local.rotation,
    transform.local.scale
  );

  const auto _fInstance = transform._fInstance;
  runtime_assert(_fInstance.isValid(), fmt::format(
    "[{}] Transform({}) instance({}) is not valid",
    __FUNCTION__, entityId, _fInstance.asValue()
  ));

  // Set the transform
  tm->setTransform(_fInstance, localMatrix);
  transform.global.matrix = tm->getWorldTransform(_fInstance);
  transform.SetDirty(false); // reset
}

void TransformSystem::setParent(
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
      spdlog::warn("[{}] New parent entity is the same as the current Parent entity ({}), skipping reparenting.",
                    __FUNCTION__, parent_fEntity.getId());
      return;
    }

    // Skip parenting if parent and child are the same
    if(fEntity == parent_fEntity) {
      spdlog::warn("[{}] New parent entity is the same as the child entity ({}), skipping reparenting.",
                    __FUNCTION__, fEntity.getId());
      return;
    }

    // Check if the parent instance is valid
    runtime_assert(
      parent_fInstance.isValid(),
      fmt::format(
        "[{}] Parent instance {} of {} is not valid.",
        __FUNCTION__, parent_fEntity.getId(), fEntity.getId()
      )
    );

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

void TransformSystem::setParent(
  const FilamentEntity& child,
  const FilamentEntity* parent
) {
  // Get the instance of the child and parent
  const auto childInstance = tm->getInstance(child);
  const auto parentInstance = !!parent ? tm->getInstance(*parent) : FilamentTransformInstance();

  // Check if the child instance is valid
  runtime_assert(
    childInstance.isValid(),
    fmt::format(
      "[{}] Child instance {} of {} is not valid.",
      __FUNCTION__, child.getId(), parent->getId()
    )
  );
  // Check if the parent instance is valid (or if null for deparenting)
  runtime_assert(
    parentInstance.isValid() || parentInstance.asValue() == 0,
    fmt::format(
      "[{}] Parent instance {} of {} is not valid.",
      __FUNCTION__, parent->getId(), child.getId()
    )
  );
  // Check if the parent and child are the same
  if (child == *parent) {
    spdlog::warn("[{}] New parent entity is the same as the child entity ({}), skipping reparenting.",
                  __FUNCTION__, child.getId());
    return;
  }

  // Set the parent transform
  tm->setParent(childInstance, parentInstance);
  
}

}  // namespace plugin_filament_view
