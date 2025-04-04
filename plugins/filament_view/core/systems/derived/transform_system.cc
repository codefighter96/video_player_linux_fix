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
  spdlog::debug("TransformSystem processing messages");
}

void TransformSystem::vHandleMessage(const ECSMessage& msg) {
  // Handle incoming messages
  spdlog::debug("TransformSystem handling message");
}

void TransformSystem::DebugPrint() {
  spdlog::debug("TransformSystem DebugPrint");
}

//
// Internal logic 
//

void TransformSystem::updateTransforms() {
  // Update transforms
  spdlog::debug("TransformSystem updating transforms");
}

void TransformSystem::updateFilamentParentTree() {
  // Update Filament parent tree
  spdlog::debug("TransformSystem updating Filament parent tree");
}

}  // namespace plugin_filament_view
