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
#include "renderable_entityobject.h"
#include <core/systems/ecs.h>
#include <core/systems/derived/material_system.h>
#include <core/include/literals.h>
#include <core/utils/asserts.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

//////////////////////////////////////////////////////////////////////////////////////////
void RenderableEntityObject::deserializeFrom(const flutter::EncodableMap& params) {
  EntityObject::deserializeFrom(params);

  // Transform (required)
  spdlog::debug("Making Transform...");
  _tmpTransform = std::make_shared<BaseTransform>(params);

  // CommonRenderable (required)
  spdlog::debug("Making CommonRenderable...");
  _tmpCommonRenderable = std::make_shared<CommonRenderable>(params);

  // Collidable (optional)
  spdlog::debug("Making Collidable...");
  if (const auto it = params.find(flutter::EncodableValue(kCollidable));
      it != params.end() && !it->second.IsNull()) {
    // They're requesting a collidable on this object. Make one.
    _tmpCollidable = std::make_shared<Collidable>(params);
  } else {
    spdlog::debug("This entity params has no collidable");
  }
  
} // namespace plugin_filament_view

void RenderableEntityObject::onInitialize() {
  checkInitialized();
  spdlog::debug("RenderableEntityObject::onInitialize - transfer components");
  spdlog::debug("ecs addr is 0x{:x}", reinterpret_cast<uintptr_t>(&ecs));

  // Transform (required)
  spdlog::debug("addComponent transform");
  runtime_assert(_tmpTransform != nullptr, "Missing transform! Did deserialization fail?");
  ecs->addComponent(guid_, std::move(_tmpTransform)); // move clears the pointer, so we're good
  debug_assert(_tmpTransform == nullptr, "We still have a transform pointer!");
  debug_assert(ecs->hasComponent<BaseTransform>(guid_), "We should have a transform component!");

  // CommonRenderable (required)
  spdlog::debug("addComponent CommonRenderable");
  runtime_assert(_tmpCommonRenderable != nullptr, "Missing CommonRenderable! Did deserialization fail?");
  ecs->addComponent(guid_, std::move(_tmpCommonRenderable));

  // Collidable (optional)
  if (_tmpCollidable != nullptr) {
    spdlog::debug("addComponent Collidable");
    runtime_assert(_tmpCollidable != nullptr, "Missing Collidable! Did deserialization fail?");
    ecs->addComponent(guid_, std::move(_tmpCollidable));
  } 
}

////////////////////////////////////////////////////////////////////////////
void RenderableEntityObject::vLoadMaterialDefinitionsToMaterialInstance() {
  checkInitialized();
  const auto materialSystem = ecs->getSystem<MaterialSystem>("RenderableEntityObject::vBuildRenderable");

  // this will also set all the default values of the material instance from
  // the material param list

  const auto materialDefinitions = GetComponent<MaterialDefinitions>();
  if (materialDefinitions != nullptr) {
    m_poMaterialInstance = materialSystem->getMaterialInstance(
        dynamic_cast<const MaterialDefinitions*>(materialDefinitions.get()));
  } else {
    spdlog::error("MaterialDefinitions is null.");
  }

  if (m_poMaterialInstance.getStatus() != Status::Success) {
    spdlog::error("Failed to get material instance.");
  }
}

} // namespace plugin_filament_view