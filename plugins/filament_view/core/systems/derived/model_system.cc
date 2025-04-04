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
#include "model_system.h"
#include "collision_system.h"
#include "filament_system.h"

#include <core/components/derived/collidable.h>
#include <core/include/file_utils.h>
#include <core/systems/ecs.h>
#include <core/utils/entitytransforms.h>
#include <curl_client/curl_client.h>
#include <filament/Scene.h>
#include <filament/filament/RenderableManager.h>
#include <filament/gltfio/ResourceLoader.h>
#include <filament/gltfio/TextureProvider.h>
#include <filament/gltfio/materials/uberarchive.h>
#include <filament/utils/Slice.h>
#include <algorithm>  // for max
#include <asio/post.hpp>

#include "animation_system.h"

namespace plugin_filament_view {

using filament::gltfio::AssetConfiguration;
using filament::gltfio::AssetLoader;
using filament::gltfio::ResourceConfiguration;
using filament::gltfio::ResourceLoader;

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::destroyAllAssetsOnModels() {
  for (const auto& [fst, snd] : m_mapszoAssets) {
    destroyAsset(snd->getAsset());  // NOLINT
  }
  m_mapszoAssets.clear();
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::destroyAsset(
    const filament::gltfio::FilamentAsset* asset) const {
  if (!asset) {
    return;
  }

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          __FUNCTION__);

  filamentSystem->getFilamentScene()->removeEntities(asset->getEntities(),
                                                     asset->getEntityCount());
  assetLoader_->destroyAsset(asset);
}

////////////////////////////////////////////////////////////////////////////////////
filament::gltfio::FilamentAsset* ModelSystem::poFindAssetByGuid(
    const EntityGUID guid) {
  const auto iter = m_mapszoAssets.find(guid);
  if (iter == m_mapszoAssets.end()) {
    return nullptr;
  }

  return iter->second->getAsset();
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::loadModelGlb(std::shared_ptr<Model> oOurModel,
                               const std::vector<uint8_t>& buffer,
                               const std::string& /*assetName*/) {
  if (assetLoader_ == nullptr) {
    // NOTE, this should only be temporary until CustomModelViewer isn't
    // necessary in implementation.
    vOnInitSystem();

    if (assetLoader_ == nullptr) {
      spdlog::error("unable to initialize model system");
      return;
    }
  }

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "loadModelGlb");
  const auto engine = filamentSystem->getFilamentEngine();
  auto& rcm = engine->getRenderableManager();

  // Note if you're creating a lot of instances, this is better to use at the
  // start FilamentAsset* createInstancedAsset(const uint8_t* bytes, uint32_t
  // numBytes, FilamentInstance **instances, size_t numInstances)
  filament::gltfio::FilamentAsset* asset = nullptr;
  filament::gltfio::FilamentInstance* assetInstance = nullptr;

  const auto instancedModelData =
      m_mapInstanceableAssets_.find(oOurModel->szGetAssetPath());
  if (instancedModelData != m_mapInstanceableAssets_.end()) {
    // we have the model already, use it.
    assetInstance = assetLoader_->createInstance(instancedModelData->second);

    const Entity* instanceEntities = assetInstance->getEntities();
    const size_t instanceEntityCount = assetInstance->getEntityCount();

    for (size_t i = 0; i < instanceEntityCount; i++) {
      const Entity entity = instanceEntities[i];

      // Check if this entity has a Renderable component
      if (rcm.hasComponent(entity)) {
        const auto ri = rcm.getInstance(entity);

        rcm.setCastShadows(
            ri, oOurModel->GetCommonRenderable()->IsCastShadowsEnabled());
        rcm.setReceiveShadows(
            ri, oOurModel->GetCommonRenderable()->IsReceiveShadowsEnabled());
        rcm.setScreenSpaceContactShadows(ri, false);
      }

      filamentSystem->getFilamentScene()->addEntity(entity);
    }

    filamentSystem->getFilamentScene()->addEntity(assetInstance->getRoot());
    oOurModel->setAssetInstance(assetInstance);
  }

  // instance-able / primary object.
  if (assetInstance == nullptr) {
    asset = assetLoader_->createAsset(buffer.data(),
                                      static_cast<uint32_t>(buffer.size()));
    if (!asset) {
      spdlog::error("Failed to loadModelGlb->createasset from buffered data.");
      return;
    }

    oOurModel->setAsset(asset);
    resourceLoader_->asyncBeginLoad(asset);

    if (oOurModel->bShouldKeepAssetDataInMemory()) {
      m_mapInstanceableAssets_.insert(
          std::pair(oOurModel->szGetAssetPath(), asset));
    } else {
      asset->releaseSourceData();
    }

    utils::Slice const listOfRenderables{asset->getRenderableEntities(),
                                         asset->getRenderableEntityCount()};

    for (const auto entity : listOfRenderables) {
      const auto ri = rcm.getInstance(entity);
      rcm.setCastShadows(
          ri, oOurModel->GetCommonRenderable()->IsCastShadowsEnabled());
      rcm.setReceiveShadows(
          ri, oOurModel->GetCommonRenderable()->IsReceiveShadowsEnabled());
      // Investigate this more before making it a property on common renderable
      // component.
      rcm.setScreenSpaceContactShadows(ri, false);
    }


    // get primary instance
    assetInstance = asset->getInstance();
    if(assetInstance == nullptr) {
      throw std::runtime_error(
          "[ModelSystem::loadModelGlb] Failed to fetch primary asset instance.");
    }

    oOurModel->setAssetInstance(assetInstance);
  }

  // by this point we should either have a subinstance, or THE primary instance
  assert(assetInstance != nullptr);

  EntityTransforms::vApplyTransform(assetInstance->getRoot(), *oOurModel->GetBaseTransform());

  std::shared_ptr<Model> sharedPtr = std::move(oOurModel);
  vSetupAssetThroughoutECS(sharedPtr, asset, assetInstance);
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::loadModelGltf(
    std::shared_ptr<Model> oOurModel,
    const std::vector<uint8_t>& buffer,
    std::function<const filament::backend::BufferDescriptor&(
        std::string uri)>& /* callback */) {

  throw std::runtime_error(
      "[ModelSystem::loadModelGltf] Not implemented yet.");

  auto* asset = assetLoader_->createAsset(buffer.data(),
                                          static_cast<uint32_t>(buffer.size()));
  if (!asset) {
    spdlog::error("Failed to loadModelGltf->createasset from buffered data.");
    return;
  }

  const auto uri_data = asset->getResourceUris();
  const auto uris =
      std::vector(uri_data, uri_data + asset->getResourceUriCount());
  for (const auto uri : uris) {
    (void)uri;
    SPDLOG_DEBUG("resource uri: {}", uri);
#if 0   // TODO
              auto resourceBuffer = callback(uri);
              if (!resourceBuffer) {
                  this->asset_ = nullptr;
                  return;
              }
              resourceLoader_->addResourceData(uri, resourceBuffer);
#endif  // TODO
  }
  resourceLoader_->asyncBeginLoad(asset);
  // modelViewer->setAnimator(asset->getInstance()->getAnimator());
  asset->releaseSourceData();

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "loadModelGltf");
  const auto engine = filamentSystem->getFilamentEngine();

  auto& rcm = engine->getRenderableManager();

  utils::Slice const listOfRenderables{asset->getRenderableEntities(),
                                       asset->getRenderableEntityCount()};

  for (const auto entity : listOfRenderables) {
    const auto ri = rcm.getInstance(entity);
    rcm.setCastShadows(
        ri, oOurModel->GetCommonRenderable()->IsCastShadowsEnabled());
    rcm.setReceiveShadows(
        ri, oOurModel->GetCommonRenderable()->IsReceiveShadowsEnabled());
    // Investigate this more before making it a property on common renderable
    // component.
    rcm.setScreenSpaceContactShadows(ri, false);
  }

  oOurModel->setAsset(asset);

  // TODO: fix this, we should be able to use the asset instance
  EntityTransforms::vApplyTransform(oOurModel->getAssetInstance()->getRoot(), *oOurModel->GetBaseTransform());

  std::shared_ptr<Model> sharedPtr = std::move(oOurModel);

  vSetupAssetThroughoutECS(sharedPtr, asset, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vSetupAssetThroughoutECS(
    std::shared_ptr<Model>& sharedPtr,
    filament::gltfio::FilamentAsset* filamentAsset,
    filament::gltfio::FilamentInstance* filamentAssetInstance) {
  m_mapszoAssets.insert(std::pair(sharedPtr->GetGuid(), sharedPtr));

  filament::gltfio::Animator* animatorInstance = nullptr;

  if (filamentAssetInstance != nullptr) {
    animatorInstance = filamentAssetInstance->getAnimator();
  } else if (filamentAsset != nullptr) {
    animatorInstance = filamentAsset->getInstance()->getAnimator();
  }

  if (animatorInstance != nullptr &&
      sharedPtr->HasComponentByStaticTypeID(Component::StaticGetTypeID<Animation>())) {
    const auto animatorComponent =
        sharedPtr->GetComponentByStaticTypeID(Component::StaticGetTypeID<Animation>());
    const auto animator = dynamic_cast<Animation*>(animatorComponent.get());
    animator->vSetAnimator(*animatorInstance);

    // Great if you need help with your animation information!
    // animationPtr->DebugPrint("From ModelSystem::vSetupAssetThroughoutECS\t");
  } else if (animatorInstance != nullptr &&
             animatorInstance->getAnimationCount() > 0) {
    SPDLOG_DEBUG(
        "For asset - {} you have a valid set of animations [{}] you can play "
        "on this, but you didn't load an animation component, load one if you "
        "want that "
        "functionality",
        sharedPtr->szGetAssetPath(), animatorInstance->getAnimationCount());
  }

  sharedPtr->vRegisterEntity();
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::populateSceneWithAsyncLoadedAssets(const Model* model) {
  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          __FUNCTION__);
  const auto engine = filamentSystem->getFilamentEngine();

  auto& rcm = engine->getRenderableManager();

  auto* asset = model->getAsset();

  if (!asset) {
    return;
  }

  size_t count = asset->popRenderables(nullptr, 0);
  while (count) {
    constexpr size_t maxToPopAtOnce = 128;
    auto maxToPop = std::min(count, maxToPopAtOnce);

    SPDLOG_DEBUG(
        "ModelSystem::populateSceneWithAsyncLoadedAssets async load count "
        "available[{}] - working on [{}]",
        count, maxToPop);
    // Note for high amounts, we should probably do a small amount; break out;
    // and let it come back do more on another frame.

    asset->popRenderables(readyRenderables_, maxToPop);

    utils::Slice const listOfRenderables{asset->getRenderableEntities(),
                                         asset->getRenderableEntityCount()};

    for (const auto entity : listOfRenderables) {
      const auto ri = rcm.getInstance(entity);
      rcm.setCastShadows(ri,
                         model->GetCommonRenderable()->IsCastShadowsEnabled());
      rcm.setReceiveShadows(
          ri, model->GetCommonRenderable()->IsReceiveShadowsEnabled());
      // Investigate this more before making it a property on common renderable
      // component.
      rcm.setScreenSpaceContactShadows(ri, false);
    }

    // we won't load the primary asset to render.
    if (!model->bIsPrimaryAssetToInstanceFrom()) {
      filamentSystem->getFilamentScene()->addEntities(readyRenderables_,
                                                      maxToPop);
    }

    count = asset->popRenderables(nullptr, 0);
  }

  if ([[maybe_unused]] auto lightEntities = asset->getLightEntities()) {
    spdlog::info(
        "Note: Light entities have come in from asset model load;"
        "these are not attached to our entities and will be un changeable");
    filamentSystem->getFilamentScene()->addEntities(asset->getLightEntities(),
                                                    sizeof(*lightEntities));
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::updateAsyncAssetLoading() {
  resourceLoader_->asyncUpdateLoad();

  // This does not specify per resource, but a global, best we can do with this
  // information is if we're done loading <everything> that was marked as async
  // load, then load that physics data onto a collidable if required. This gives
  // us visuals without collidables in a scene with <tons> of objects; but would
  // eventually settle
  const float percentComplete = resourceLoader_->asyncGetLoadProgress();

  for (const auto& [guid, asset] : m_mapszoAssets) {
    populateSceneWithAsyncLoadedAssets(asset.get());

    if (percentComplete != 1.0f) {
      continue;
    }

    auto assetPath = asset->szGetAssetPath();

    // we're no longer loading, we're loaded.
    m_mapszbCurrentlyLoadingInstanceableAssets.erase(assetPath);

    // once we're done loading our assets; we should be able to load instanced
    // models.
    if (auto foundAwaitingIter = m_mapszoAssetsAwaitingDataLoad.find(assetPath);
        asset->bShouldKeepAssetDataInMemory() &&
        foundAwaitingIter != m_mapszoAssetsAwaitingDataLoad.end()) {
      spdlog::info("Loading additional instanced assets: {}", assetPath);
      for (const auto& itemToLoad : foundAwaitingIter->second) {
        spdlog::info("Loading subset: {}", assetPath);
        std::vector<uint8_t> emptyVec;

        // retry loading the asset, as we're done loading the instanceable data
        loadModelGlb(itemToLoad, emptyVec, itemToLoad->szGetAssetPath());
      }
      spdlog::info("Done Loading additional instanced assets: {}", assetPath);
      m_mapszoAssetsAwaitingDataLoad.erase(assetPath);
    }

    // You don't get collision as a primary asset.
    if (asset->bIsPrimaryAssetToInstanceFrom()) {
      continue;
    }

    auto collisionSystem =
        ecs->getSystem<CollisionSystem>(
            "updateAsyncAssetLoading");
    if (collisionSystem == nullptr) {
      spdlog::warn("Failed to get collision system when loading model");
      continue;
    }

    // if its 'done' loading, we need to create our large AABB collision
    // object if this model it's referencing required one.
    //
    // Also need to make sure it hasn't already created one for this model.
    if (asset->HasComponentByStaticTypeID(Component::StaticGetTypeID<Collidable>()) &&
        !collisionSystem->bHasEntityObjectRepresentation(guid)) {
      // I don't think this needs to become a message; as an async load
      // gives us un-deterministic throughput; it can't be replicated with a
      // messaging structure, and we have to wait till the load is done.
      collisionSystem->vAddCollidable(asset.get());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> ModelSystem::loadGlbFromAsset(
    std::shared_ptr<Model> oOurModel,
    const std::string& path) {
  const auto promise(
      std::make_shared<std::promise<Resource<std::string_view>>>());
  auto promise_future(promise->get_future());

  try {
    const asio::io_context::strand& strand_(
        *ecs->GetStrand());

    const auto assetPath =
        ecs->getConfigValue<std::string>(kAssetPath);

    const bool bWantsInstancedData = oOurModel->bShouldKeepAssetDataInMemory();
    const bool hasInstancedDataLoaded =
        m_mapInstanceableAssets_.find(oOurModel->szGetAssetPath()) !=
        m_mapInstanceableAssets_.end();
    const bool isCurrentlyLoadingInstanceableData =
        m_mapszbCurrentlyLoadingInstanceableAssets.find(
            oOurModel->szGetAssetPath()) !=
        m_mapszbCurrentlyLoadingInstanceableAssets.end();

    // if we are loading instanceable data...
    if (bWantsInstancedData) {
      std::string szAssetPath = oOurModel->szGetAssetPath();
      // ... and we're currently loading it or it's already loaded
      // add it to the list of assets to update when we're done loading.
      // see: updateAsyncAssetLoading() (which the calls this again)
      if (isCurrentlyLoadingInstanceableData || hasInstancedDataLoaded) {
        const auto iter =
            m_mapszoAssetsAwaitingDataLoad.find(oOurModel->szGetAssetPath());

        // find/create a list of instances to load
        // and add this model to it.
        if (iter != m_mapszoAssetsAwaitingDataLoad.end()) {
          iter->second.emplace_back(std::move(oOurModel));
        } else {
          std::list<std::shared_ptr<Model>> modelListToLoad;
          modelListToLoad.emplace_back(std::move(oOurModel));
          m_mapszoAssetsAwaitingDataLoad.insert(
              std::pair(szAssetPath, modelListToLoad));
        }

        promise->set_value(Resource<std::string_view>::Success(
            "Waiting Data load from other asset load adding to list to update "
            "during update tick."));

        return promise_future;
      }

      // if we're not loading instance data, this will fall out and load it, set
      // that we're loading it. next model will see its loading. and add itself
      // to the wait.
      m_mapszbCurrentlyLoadingInstanceableAssets.insert(
          std::pair(szAssetPath, true));
    }

    post(strand_,
         [&, model = std::move(oOurModel), promise, path, assetPath]() mutable {
           try {
             const auto buffer = readBinaryFile(path, assetPath);
             handleFile(std::move(model), buffer, path, promise);
           } catch (const std::exception& e) {
             spdlog::warn("Lambda Exception {}", e.what());
             promise->set_exception(std::make_exception_ptr(e));
           } catch (...) {
             spdlog::warn("Unknown Exception in lambda");
           }
         });
  } catch (const std::exception& e) {
    std::cerr << "Total Exception: " << e.what() << '\n';
    promise->set_exception(std::make_exception_ptr(e));
  }
  return promise_future;
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> ModelSystem::loadGlbFromUrl(
    std::shared_ptr<Model> oOurModel,
    std::string url) {
  const auto promise(
      std::make_shared<std::promise<Resource<std::string_view>>>());
  auto promise_future(promise->get_future());
  post(*ecs->GetStrand(),
       [&, model = std::move(oOurModel), promise,
        url = std::move(url)]() mutable {
         plugin_common_curl::CurlClient client;
         const auto buffer = client.RetrieveContentAsVector();
         if (client.GetCode() != CURLE_OK) {
           promise->set_value(Resource<std::string_view>::Error(
               "Couldn't load Glb from " + url));
         }
         handleFile(std::move(model), buffer, url, promise);
       });
  return promise_future;
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::handleFile(std::shared_ptr<Model>&& oOurModel,
                             const std::vector<uint8_t>& buffer,
                             const std::string& fileSource,
                             const PromisePtr& promise) {
  spdlog::debug("handleFile");
  if (!buffer.empty()) {
    loadModelGlb(std::move(oOurModel), buffer, fileSource);
    promise->set_value(Resource<std::string_view>::Success(
        "Loaded glb model successfully from " + fileSource));
  } else {
    promise->set_value(Resource<std::string_view>::Error(
        "Couldn't load glb model from " + fileSource));
  }
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> ModelSystem::loadGltfFromAsset(
    const std::shared_ptr<Model>& /*oOurModel*/,
    const std::string& /* path */,
    const std::string& /* pre_path */,
    const std::string& /* post_path */) {
  const auto promise(
      std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());
  promise->set_value(Resource<std::string_view>::Error("Not implemented yet"));
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> ModelSystem::loadGltfFromUrl(
    const std::shared_ptr<Model>& /*oOurModel*/,
    const std::string& /* url */) {
  const auto promise(
      std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());
  promise->set_value(Resource<std::string_view>::Error("Not implemented yet"));
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vOnInitSystem() {
  if (materialProvider_ != nullptr) {
    return;
  }

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "ModelSystem::vInitSystem");
  const auto engine = filamentSystem->getFilamentEngine();

  if (engine == nullptr) {
    spdlog::error("Engine is null, delaying vInitSystem");
    return;
  }

  materialProvider_ = filament::gltfio::createUbershaderProvider(
      engine, UBERARCHIVE_DEFAULT_DATA,
      static_cast<size_t>(UBERARCHIVE_DEFAULT_SIZE));

  SPDLOG_DEBUG("UbershaderProvider MaterialsCount: {}",
               materialProvider_->getMaterialsCount());

  AssetConfiguration assetConfiguration{};
  assetConfiguration.engine = engine;
  assetConfiguration.materials = materialProvider_;
  assetLoader_ = AssetLoader::create(assetConfiguration);

  ResourceConfiguration resourceConfiguration{};
  resourceConfiguration.engine = engine;
  resourceConfiguration.normalizeSkinningWeights = true;
  resourceLoader_ = new ResourceLoader(resourceConfiguration);

  const auto decoder = filament::gltfio::createStbProvider(engine);
  resourceLoader_->addTextureProvider("image/png", decoder);
  resourceLoader_->addTextureProvider("image/jpeg", decoder);

  // ChangeTranslationByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeTranslationByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeTranslationByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeTranslationByGUID);

        const auto position =
            msg.getData<filament::math::float3>(ECSMessageType::floatVec3);

        // find the model in our list:
        if (const auto ourEntity = m_mapszoAssets.find(guid);
            ourEntity != m_mapszoAssets.end()) {
          const auto model = ourEntity->second;
          const auto transform = dynamic_cast<BaseTransform*>(
            model
                  ->GetComponentByStaticTypeID(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetPosition(position);

          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);

          // and change the collision
          vRemoveAndReaddModelToCollisionSystem(ourEntity->first,
                                                model);
        }

        SPDLOG_TRACE("ChangeTranslationByGUID Complete");
      });

  // ChangeRotationByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeRotationByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeRotationByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeRotationByGUID);

        const auto values =
            msg.getData<filament::math::float4>(ECSMessageType::floatVec4);
        filament::math::quatf rotation(values);

        // find the model in our list:
        if (const auto ourEntity = m_mapszoAssets.find(guid);
            ourEntity != m_mapszoAssets.end()) {
          const auto model = ourEntity->second;
          const auto transform = dynamic_cast<BaseTransform*>(
              model
                  ->GetComponentByStaticTypeID(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetRotation(rotation);

          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);

          // and change the collision
          vRemoveAndReaddModelToCollisionSystem(ourEntity->first,
                                                model);
        }

        SPDLOG_TRACE("ChangeRotationByGUID Complete");
      });

  // ChangeScaleByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeScaleByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeScaleByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeScaleByGUID);

        const auto values =
            msg.getData<filament::math::float3>(ECSMessageType::floatVec3);

        // find the model in our list:
        if (const auto ourEntity = m_mapszoAssets.find(guid);
            ourEntity != m_mapszoAssets.end()) {
          const auto model = ourEntity->second;
          const auto transform = dynamic_cast<BaseTransform*>(
              model
                  ->GetComponentByStaticTypeID(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetScale(values);

          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);

          // and change the collision
          vRemoveAndReaddModelToCollisionSystem(ourEntity->first,
                                                model);
        }

        SPDLOG_TRACE("ChangeScaleByGUID Complete");
      });

  vRegisterMessageHandler(
      ECSMessageType::ToggleVisualForEntity, [this](const ECSMessage& msg) {
        spdlog::debug("ToggleVisualForEntity");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ToggleVisualForEntity);
        const auto value = msg.getData<bool>(ECSMessageType::BoolValue);

        if (const auto ourEntity = m_mapszoAssets.find(guid);
            ourEntity != m_mapszoAssets.end()) {
          const auto fSystem =
              ecs->getSystem<FilamentSystem>(
                  "vRegisterMessageHandler::ToggleVisualForEntity");

          if (const auto modelAsset = ourEntity->second->getAsset()) {
            if (value) {
              fSystem->getFilamentScene()->addEntities(
                  modelAsset->getRenderableEntities(),
                  modelAsset->getRenderableEntityCount());
            } else {
              fSystem->getFilamentScene()->removeEntities(
                  modelAsset->getRenderableEntities(),
                  modelAsset->getRenderableEntityCount());
            }
          } else if (const auto modelAssetInstance =
                         ourEntity->second->getAssetInstance()) {
            if (value) {
              fSystem->getFilamentScene()->addEntities(
                  modelAssetInstance->getEntities(),
                  modelAssetInstance->getEntityCount());
            } else {
              fSystem->getFilamentScene()->removeEntities(
                  modelAssetInstance->getEntities(),
                  modelAssetInstance->getEntityCount());
            }
          }
        }

        spdlog::debug("ToggleVisualForEntity Complete");
      });
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vRemoveAndReaddModelToCollisionSystem(
    const EntityGUID guid,
    const std::shared_ptr<Model>& model) {
  const auto collisionSystem =
      ecs->getSystem<CollisionSystem>(
          "vRemoveAndReaddModelToCollisionSystem");
  if (collisionSystem == nullptr) {
    spdlog::warn(
        "Failed to get collision system when "
        "vRemoveAndReaddModelToCollisionSystem");
    return;
  }

  // if we are marked for collidable, have one in the scene, remove and readd
  // if this is a performance issue, we can do the transform move in the future
  // instead.
  if (model->HasComponentByStaticTypeID(Component::StaticGetTypeID<Collidable>()) &&
      collisionSystem->bHasEntityObjectRepresentation(guid)) {
    collisionSystem->vRemoveCollidable(model.get());
    collisionSystem->vAddCollidable(model.get());
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vUpdate(float /*fElapsedTime*/) {
  updateAsyncAssetLoading();
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vShutdownSystem() {
  destroyAllAssetsOnModels();
  delete resourceLoader_;
  resourceLoader_ = nullptr;

  if (assetLoader_) {
    AssetLoader::destroy(&assetLoader_);
    assetLoader_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::DebugPrint() {
  SPDLOG_DEBUG("{}", __FUNCTION__);
}

}  // namespace plugin_filament_view
