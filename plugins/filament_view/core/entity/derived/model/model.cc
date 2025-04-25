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

////////////////////////////////////////////////////////////////////////////
Model::Model(std::string assetPath,
             const flutter::EncodableMap& params)
    : RenderableEntityObject(params),
      assetPath_(std::move(assetPath)),
      m_poAsset(nullptr),
      m_poAssetInstance(nullptr) {
  Deserialize::DecodeParameterWithDefault(kRenderable_KeepAssetInMemory,
                                          &m_isInstanced,
                                          params, false);

  Deserialize::DecodeParameterWithDefault(
      kRenderable_IsPrimaryAssetToInstanceFrom,
      &m_isInstancePrimary, params, false);


  _initParams = params;
}

////////////////////////////////////////////////////////////////////////////
void Model::onInitialize() {
  checkInitialized();
  spdlog::debug("Model::onInitialize - make components");

  // Transform (required)
  spdlog::debug("Making Transform...");
  auto transform = std::make_shared<BaseTransform>(_initParams);
  spdlog::debug("addComponent transform");
  spdlog::debug("ecs addr is 0x{:x}", reinterpret_cast<uintptr_t>(&ecs));
  ecs->addComponent(guid_, std::move(transform));
  spdlog::debug("addComponent transform done");

  // Collidable (optional)
  spdlog::debug("Making Collidable...");
  if (const auto it = _initParams.find(flutter::EncodableValue(kCollidable));
      it != _initParams.end() && !it->second.IsNull()) {
    // They're requesting a collidable on this object. Make one.
    auto collidable = std::make_shared<Collidable>(_initParams);
    ecs->addComponent(guid_, std::move(collidable));
  }

  // Animation (optional)
  spdlog::debug("Making Animation...");
  if (const auto it = _initParams.find(flutter::EncodableValue(kAnimation));
      it != _initParams.end() && !it->second.IsNull()) {
    auto animation = std::make_shared<Animation>(
        std::get<flutter::EncodableMap>(it->second));
    ecs->addComponent(guid_, std::move(animation));
  }

  // CommonRenderable (required)
  spdlog::debug("Making CommonRenderable...");
  auto commonRenderable = std::make_shared<CommonRenderable>(_initParams);
  ecs->addComponent(guid_, std::move(commonRenderable));

  // TODO: Setup renderable & children, from ModelSystem::

  spdlog::debug("Model::onInitialize - done!");
}

////////////////////////////////////////////////////////////////////////////
GlbModel::GlbModel(std::string assetPath,
                   const flutter::EncodableMap& params)
    : Model(std::move(assetPath), params) {}

////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Model> Model::Deserialize(
    const std::string& /*flutterAssetsPath*/,
    const flutter::EncodableMap& params) {
  SPDLOG_TRACE("++Model::Model");
  std::unique_ptr<Animation> animation;
  std::optional<std::string> assetPath;
  bool is_glb = false;

  for (const auto& [fst, snd] : params) {
    if (snd.IsNull())
      continue;
    if (auto key = std::get<std::string>(fst);
        key == "assetPath" && std::holds_alternative<std::string>(snd)) {
      assetPath = std::get<std::string>(snd);
    } else if (key == "isGlb" && std::holds_alternative<bool>(snd)) {
      is_glb = std::get<bool>(snd);
    } else if (key == "scene" &&
               std::holds_alternative<flutter::EncodableMap>(snd)) {
      spdlog::warn("Scenes are no longer valid off of a model node.");
    } /*else if (!it.second.IsNull()) {
      spdlog::debug("[Model] Unhandled Parameter");
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(),
                                                           it.second);
    }*/
  }

  std::shared_ptr<Model> toReturn;

  if (is_glb) {
    spdlog::debug("Model::Deserialize - is_glb");
    toReturn = std::make_shared<GlbModel>(
        std::move(assetPath.value()), params
    );
  } else {
    spdlog::debug("Model::Deserialize - is_gltf");
    throw std::runtime_error("GLTF support not implemented yet.");
  }
  
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
void Model::vLoadMaterialDefinitionsToMaterialInstance() {
  const auto materialSystem =
      ECSManager::GetInstance()->getSystem<MaterialSystem>(
          "BaseShape::vBuildRenderable");

  if (materialSystem == nullptr) {
    spdlog::error("Failed to get material system.");
  } else {
    // this will also set all the default values of the material instance from
    // the material param list
    const auto materialDefinitions =
        GetComponent(Component::StaticGetTypeID<MaterialDefinitions>());
    if (materialDefinitions != nullptr) {
      m_poMaterialInstance = materialSystem->getMaterialInstance(
          dynamic_cast<const MaterialDefinitions*>(materialDefinitions.get()));
    }

    if (m_poMaterialInstance.getStatus() != Status::Success) {
      spdlog::error("Failed to get material instance.");
    }
  }
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
