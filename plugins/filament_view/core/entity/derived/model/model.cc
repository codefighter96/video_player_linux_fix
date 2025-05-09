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

#include "model.h"

#include <core/components/derived/collidable.h>
#include <core/include/literals.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/material_system.h>
#include <core/systems/ecs.h>
#include <core/utils/deserialize.h>
#include <filament/RenderableManager.h>
#include <plugins/common/common.h>
#include <utils/Slice.h>
#include <utility>

namespace plugin_filament_view {

const char* modelInstancingModeToString(ModelInstancingMode mode) {
  switch (mode) {
    case ModelInstancingMode::none:
      return "none";
    case ModelInstancingMode::primary:
      return "primary";
    case ModelInstancingMode::secondary:
      return "secondary";
    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////
Model::Model()
    : RenderableEntityObject(),
      assetPath_(),
      m_poAsset(nullptr),
      m_poAssetInstance(nullptr) {}

void Model::deserializeFrom(const flutter::EncodableMap& params) {
  RenderableEntityObject::deserializeFrom(params);

  // assetPath_
  assetPath_ = Deserialize::DecodeParameter<std::string>(kAssetPath, params);

  // is_glb
  bool is_glb = Deserialize::DecodeParameter<bool>(kIsGlb, params);
  runtime_assert(is_glb, "Model::deserializeFrom - is_glb must be true for Model");

  // _instancingMode
  Deserialize::DecodeEnumParameterWithDefault(
    kModelInstancingMode,
    &_instancingMode,
    params,
    ModelInstancingMode::none
  );

  spdlog::debug("Model({}), instanceMode: {}",
                assetPath_, modelInstancingModeToString(_instancingMode));

  // Animation (optional)
  spdlog::debug("Making Animation...");
  if (Deserialize::HasKey(params, kAnimation)) {
    // they're requesting an animation on this object. Make one.
    _tmpAnimation = std::make_shared<Animation>(params);
  } else {
    spdlog::debug("This entity params has no animation");
  }
}

void Model::onInitialize() {
  RenderableEntityObject::onInitialize();

  checkInitialized();
  spdlog::debug("Model::onInitialize - transfer components");
  spdlog::debug("ecs addr is 0x{:x}", reinterpret_cast<uintptr_t>(&ecs));

  // Animation (optional)
  if (_tmpAnimation != nullptr) {
    spdlog::debug("addComponent Animation");
    ecs->addComponent(guid_, std::move(_tmpAnimation));
  }

  // TODO: Setup renderable & children, from ModelSystem::

  spdlog::debug("Model::onInitialize - done!");
}

////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Model> Model::Deserialize(const flutter::EncodableMap& params) {

  std::shared_ptr<Model> toReturn = std::make_shared<Model>();
  toReturn->deserializeFrom(params);
  
  // TODO: this should NOT be happening here
  spdlog::debug("Adding entity {} from {}", toReturn->GetGuid(), __FUNCTION__);
  auto ecs = ECSManager::GetInstance();
  ecs->addEntity(toReturn); // this now inits components, takes params from constructor

  return toReturn;
}

////////////////////////////////////////////////////////////////////////////
void Model::DebugPrint() const {
  vDebugPrintComponents();
}

////////////////////////////////////////////////////////////////////////////
void Model::vChangeMaterialDefinitions(const flutter::EncodableMap& params,
                                       const TextureMap& /*loadedTextures*/) {
  // if we have a materialdefinitions component, we need to remove it
  // and remake / add a new one.
  if (HasComponent(Component::StaticGetTypeID<MaterialDefinitions>())) {
    ecs->removeComponent(guid_, Component::StaticGetTypeID<MaterialDefinitions>());
  }

  // If you want to inspect the params coming in.
  /*for (const auto& [fst, snd] : params) {
      auto key = std::get<std::string>(fst);
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(), snd);
  }*/

  auto materialDefinitions = std::make_shared<MaterialDefinitions>(params);
  ecs->addComponent(guid_, std::move(materialDefinitions));

  m_poMaterialInstance.vReset();

  // then tell material system to load us the correct way once
  // we're deserialized.
  vLoadMaterialDefinitionsToMaterialInstance();

  if (m_poMaterialInstance.getStatus() != Status::Success) {
    spdlog::error(
        "Unable to load material definition to instance, bailing out.");
    return;
  }

  // now, reload / rebuild the material?
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "BaseShape::vChangeMaterialDefinitions");

  // If your entity has multiple primitives, you’ll need to call
  // setMaterialInstanceAt for each primitive you want to update.
  auto& renderManager =
      filamentSystem->getFilamentEngine()->getRenderableManager();

  if (getAsset()) {
    utils::Slice const listOfRenderables{
        getAsset()->getRenderableEntities(),
        getAsset()->getRenderableEntityCount()};

    // Note this will apply to EVERYTHING currently. You might want a custom
    // <only effect these pieces> type functionality.
    for (const auto entity : listOfRenderables) {
      const auto ri = renderManager.getInstance(entity);

      // I dont know about primitive index being non zero if our tree has
      // multiple nodes getting from the asset.
      renderManager.setMaterialInstanceAt(ri, 0,
                                          *m_poMaterialInstance.getData());
    }
  } else if (getAssetInstance()) {
    const utils::Entity* instanceEntities = getAssetInstance()->getEntities();
    const size_t instanceEntityCount = getAssetInstance()->getEntityCount();

    for (size_t i = 0; i < instanceEntityCount; i++) {
      // Check if this entity has a Renderable component
      if (const utils::Entity entity = instanceEntities[i];
          renderManager.hasComponent(entity)) {
        const auto ri = renderManager.getInstance(entity);

        // A Renderable can have multiple primitives (submeshes)
        const size_t submeshCount = renderManager.getPrimitiveCount(ri);
        for (size_t sm = 0; sm < submeshCount; sm++) {
          // Give the submesh our new material instance
          renderManager.setMaterialInstanceAt(ri, sm,
                                              *m_poMaterialInstance.getData());
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////
void Model::vChangeMaterialInstanceProperty(
    const MaterialParameter* materialParam,
    const TextureMap& loadedTextures) {
  if (m_poMaterialInstance.getStatus() != Status::Success) {
    spdlog::error(
        "No material definition set for model, set one first that's not the "
        "uber shader.");
    return;
  }

  const auto data = m_poMaterialInstance.getData().value();

  const auto matDefs = dynamic_cast<MaterialDefinitions*>(
      GetComponent(Component::StaticGetTypeID<MaterialDefinitions>()).get());
  if (matDefs == nullptr) {
    return;
  }

  MaterialDefinitions::vApplyMaterialParameterToInstance(data, materialParam,
                                                         loadedTextures);
}

}  // namespace plugin_filament_view
