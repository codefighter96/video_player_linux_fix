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

#include <core/components/derived/collidable.h>
#include <core/entity/derived/nonrenderable_entityobject.h>
#include <core/include/file_utils.h>
#include <core/systems/ecs.h>
#include <core/utils/entitytransforms.h>
#include <curl_client/curl_client.h>
#include <filament/Scene.h>
#include <filament/gltfio/ResourceLoader.h>
#include <filament/gltfio/TextureProvider.h>
#include <filament/gltfio/materials/uberarchive.h>
#include <filament/utils/Slice.h>
#include <algorithm>  // for max
#include <cassert>
#include <asio/post.hpp>

// rapidjson
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/rapidjson.h>

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

  _filament->getFilamentScene()->removeEntities(asset->getEntities(),
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

bool ModelSystem::setupRenderable(
  const Entity entity,
  const Model* model,
  filament::gltfio::FilamentAsset* asset
) {

  // spdlog::debug("Entity ({}){} has extras: {}", entity.getId(), name, extras);

  const auto ri = _rcm->getInstance(entity);
  const auto commonRenderable = model->GetCommonRenderable();
  _rcm->setCastShadows(ri, commonRenderable->IsCastShadowsEnabled());
  _rcm->setReceiveShadows(ri, commonRenderable->IsReceiveShadowsEnabled());
  _rcm->setScreenSpaceContactShadows(ri, false);

  // Get extras (aka "userData", aka Blender's "Custom Properties"), string containing JSON
  const char* extras = asset->getExtras(entity);
  return extras != nullptr;
}


void ModelSystem::setupCollidableChild(
  const Entity entity,
  const Model* model,
  filament::gltfio::FilamentAsset* asset
) {
  // Get name
  const char* name = asset->getName(entity);
  if(name == nullptr) {
    name = "(null)";
  }
  // Get extras (aka "userData", aka Blender's "Custom Properties"), string containing JSON
  const char* extras = asset->getExtras(entity);
  if(extras == nullptr) {
    extras = "{}";
  }

  // if extras has a "fs_touchEvent" property, get the value
  if(extras != nullptr) {
    // parse using RapidJSON
    rapidjson::Document doc;
    doc.Parse(extras);
    if(doc.HasParseError()) {
      spdlog::error(rapidjson::GetParseError_En(doc.GetParseError()));
      throw std::runtime_error(
          "[ModelSystem::setupCollidableChild] Failed to parse extras JSON");
    }

    constexpr char kTouchEventProp[] = "customDataProp";
    if(doc.HasMember(kTouchEventProp)) {
      const auto& touchEvent = doc[kTouchEventProp];
      // type: string (event name)
      const auto& type = touchEvent.GetString();
      spdlog::debug("{} type: {}", kTouchEventProp, type);

      // get aabb
      const auto ri = _rcm->getInstance(entity);
      const filament::Box aabb = _rcm->getAxisAlignedBoundingBox(ri);

      // get center
      const filament::math::float3 center = aabb.center;

      // check if is a perfect cube (with epsilon = 0.01)
      constexpr float epsilon = 0.01f;
      const float sizeX = aabb.halfExtent.x * 2;
      const float sizeY = aabb.halfExtent.y * 2;
      const float sizeZ = aabb.halfExtent.z * 2;
      const bool isCube = std::abs(sizeX - sizeY) < epsilon &&
                          std::abs(sizeY - sizeZ) < epsilon;
      spdlog::debug("isCube: {}", isCube);

      spdlog::debug("Hello????");

      // create a child entityobject
      const EntityGUID parentGuid = model->GetGuid();
      spdlog::debug("Parent entity: {}", parentGuid);
      const std::string childName = std::string(name);
      spdlog::debug("Created name");
      std::shared_ptr<EntityObject> childEntity =
          std::make_shared<NonRenderableEntityObject>(childName);
      spdlog::debug("Created child entityobject: {}", childName);
      childEntity->_fEntity = std::make_shared<utils::Entity>(_em->create());

      spdlog::debug("Created entityobject");

      const int rootEntityId = 0;
      // filament::math::mat4f localMatrix = filament::math::mat4f();
      auto en = *childEntity->_fEntity.get();
      while(en.getId() != rootEntityId) {
        spdlog::debug("Getting transform for entity {}", en.getId());

        const auto instance = _tm->getInstance(en);
        // localMatrix = filament::math::mat4f::multiply(_tm->getTransform(instance), localMatrix);
        en = _tm->getParent(instance);
      }

      spdlog::debug("yayyyyY!");


      // attach transform
      auto transform = std::make_shared<BaseTransform>();
      transform->SetPosition(center);
      transform->SetRotation(filament::math::quatf(0, 0, 0, 1));
      transform->SetScale(filament::math::float3(1, 1, 1));
      ecs->addComponent(childEntity->GetGuid(), transform);

      spdlog::debug("Added transform");

      // attach collidable
      auto collidable = std::make_shared<Collidable>();
      collidable->SetIsStatic(false);
      collidable->SetExtentsSize(filament::math::float3(sizeX, sizeY, sizeZ));
      collidable->SetShapeType(isCube ? ShapeType::Cube :
                                        ShapeType::Sphere);
      ecs->addComponent(childEntity->GetGuid(), collidable);

      spdlog::debug("Added collidable");

      // insert as child
      spdlog::debug("Adding child entity {} to model: {}",
                  childEntity->GetGuid(), parentGuid
      );
      
      EntityTransforms::vApplyTransform(
        *(childEntity->_fEntity.get()),
        transform->GetRotation(),
        transform->GetScale(),
        transform->GetPosition(),
        model->_fEntity.get()
      );
      // childEntity->vRegisterEntity();
      spdlog::debug("Adding entity {} from {}", childEntity->GetGuid(), __FUNCTION__);
      ecs->addEntity(childEntity
        // ,parentGuid
      );

      spdlog::debug("Child object '{}' added to model '{}'",
                  childEntity->GetName(), model->GetName());
      transform->DebugPrint("  ");
      collidable->DebugPrint("  ");

      spdlog::debug("Transform applied!");

      // add to collision system
      const auto collisionSystem = ecs->getSystem<CollisionSystem>("setupCollidableChild");

      // collisionSystem->vAddCollidable(childEntity.get());

      spdlog::debug("Collidable added!");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::loadModelGlb(std::shared_ptr<Model> model,
                               const std::vector<uint8_t>& buffer
) {
  if (assetLoader_ == nullptr) {
    // NOTE, this should only be temporary until CustomModelViewer isn't
    // necessary in implementation.
    vOnInitSystem();

    if (assetLoader_ == nullptr) {
      spdlog::error("unable to initialize model system");
      return;
    }
  }

  const auto assetPath = model->szGetAssetPath();
  spdlog::debug("ModelSystem::loadModelGlb: {}", assetPath);

  // Note if you're creating a lot of instances, this is better to use at the
  // start FilamentAsset* createInstancedAsset(const uint8_t* bytes, uint32_t
  // numBytes, FilamentInstance **instances, size_t numInstances)
  filament::gltfio::FilamentAsset* asset = nullptr;
  filament::gltfio::FilamentInstance* assetInstance = nullptr;

  std::vector<Entity> collidableChildren = {};

  // check if instanceable
  const auto instancedModelData = m_mapInstanceableAssets_.find(assetPath);
  if (instancedModelData != m_mapInstanceableAssets_.end()) {
    asset = instancedModelData->second;
    assetInstance = assetLoader_->createInstance(asset);
  }

  // instance-able / primary object.
  if (assetInstance == nullptr) {
    asset = assetLoader_->createAsset(buffer.data(),
                                      static_cast<uint32_t>(buffer.size()));
    if (!asset) {
      spdlog::error("Failed to loadModelGlb->createasset from buffered data.");
      return;
    }

    model->setAsset(asset);
    resourceLoader_->asyncBeginLoad(asset);

    if (model->isInstanced()) {
      m_mapInstanceableAssets_.insert(
          std::pair(assetPath, asset));
    } else {
      asset->releaseSourceData();
    }
    
    // get primary instance
    assetInstance = asset->getInstance();
    if(assetInstance == nullptr) {
      throw std::runtime_error(
          "[ModelSystem::loadModelGlb] Failed to fetch primary asset instance.");
    }

    model->setAssetInstance(assetInstance);
  }
  // instance / secondary object.
  else {
    model->setAssetInstance(assetInstance);
    addModelToScene(model.get(), true);
  }

  // TODO: move the stuff below to `addModelToScene`,
  // if not done it will cause non-instanceables to:
  // - have improper initial transforms
  // - not have additional components configured (see `vSetupAssetThroughoutECS`)

  // by this point we should either have a subinstance, or THE primary instance
  debug_assert(assetInstance != nullptr, "This should NEVER be null!");
  std::shared_ptr<Model> sharedPtr = std::move(model);
  vSetupAssetThroughoutECS(sharedPtr);

  // Set up collidable children
  // for (const auto entity : collidableChildren) {
  //   setupCollidableChild(entity, sharedPtr.get(), asset);
  // }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vSetupAssetThroughoutECS(
    std::shared_ptr<Model>& model) {
  m_mapszoAssets.insert(std::pair(model->GetGuid(), model));

  // Add to ECS
  spdlog::debug("Adding entity {} from {}", model->GetGuid(), __FUNCTION__);
  ecs->addEntity(model);

  // Get asset data
  auto asset = model->getAsset();
  auto assetInstance = model->getAssetInstance();

  // Set up transform
  EntityTransforms::vApplyTransform(assetInstance->getRoot(), *model->GetBaseTransform());

  // Set up animator
  filament::gltfio::Animator* animatorInstance = nullptr;
  if (assetInstance != nullptr) {
    animatorInstance = assetInstance->getAnimator();
  } else if (asset != nullptr) {
    animatorInstance = asset->getInstance()->getAnimator();
  }
  if (animatorInstance != nullptr &&
      model->HasComponent(Component::StaticGetTypeID<Animation>())) {
    const auto animatorComponent =
        model->GetComponent(Component::StaticGetTypeID<Animation>());
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
        model->szGetAssetPath(), animatorInstance->getAnimationCount());
  }
}

void ModelSystem::addModelToScene(
  Model* model,
  bool isInstanced
) {
  const bool isInScene = model->isInScene();
  // TODO: get `isInstanced` from the `model` instead. Currently `isInstanced` is UNRELIABLE!

  if(isInScene) {
    spdlog::warn(
      "[{}] model '{}'({}) is already in scene (asset {}), skipping add",
      __FUNCTION__, model->GetName(), model->GetGuid(), model->assetPath_
    );
    return;
  }

  Entity* modelEntities = nullptr;
  size_t modelEntityCount = 0;

  if(isInstanced) {
    runtime_assert(
      model->getAssetInstance() != nullptr,
      "ModelSystem::addModelToScene: model asset instance cannot be null"
    );
    runtime_assert(
      model->getAsset() == nullptr,
      "ModelSystem::addModelToScene: model asset should be null"
    );

    modelEntities = const_cast<utils::Entity*>(model->getAssetInstance()->getEntities());
    modelEntityCount = model->getAssetInstance()->getEntityCount();
  } else {
    // runtime_assert(
    //   model->getAsset() != nullptr,
    //   "ModelSystem::addModelToScene: model asset cannot be null"
    // );
    if(model->getAsset() == nullptr) {
      spdlog::warn(
        "ModelSystem::addModelToScene: model asset is null ({}), "
        "deferring load till later",
        model->assetPath_
      );
      return;
    }

    modelEntities = const_cast<utils::Entity*>(model->getAsset()->getRenderableEntities());
    modelEntityCount = model->getAsset()->getRenderableEntityCount();
  }

  /*
   *  Renderable setup
   */
  utils::Slice const renderables{
    modelEntities,
    modelEntityCount
  };

  for (const auto entity : renderables) {
    // const auto entity = modelEntities[i];
    _filament->getFilamentScene()->addEntity(entity);

    // TODO: move this elsewhere to enable creation of proper children
    // const bool hasCollidableChild = 
      setupRenderable(
        entity,
        model,
        const_cast<filament::gltfio::FilamentAsset*>(
          model->getAssetInstance()->getAsset()
        )
      );

    // if (hasCollidableChild) {
    //   collidableChildren.push_back(entity);
    // }

    // // print name
    // const char* name = asset->getName(entity);
    // if(name == nullptr) {
    //   name = "(null)";
    // }
    // spdlog::debug("Entity ({}){}", entity.getId(), name);

    // // print position, scale, rotation
    // const auto instance = _tm->getInstance(entity);
    // const auto transform = _tm->getWorldTransform(instance);
    // const auto position = EntityTransforms::oGetTranslationFromTransform(transform);
    // const auto scale = EntityTransforms::oGetScaleFromTransform(transform);
    // // const auto rotation = EntityTransforms::oGetRotationFromTransform(transform);
    // spdlog::debug("Position: ({}, {}, {})", position.x, position.y, position.z);
    // spdlog::debug("Scale: ({}, {}, {})", scale.x, scale.y, scale.z);
    // // spdlog::debug("Rotation: ({}, {}, {}, {})", rotation.x, rotation.y, rotation.z, rotation.w);

    // // print parent id
    // const auto parent = _tm->getParent(instance);
    // spdlog::debug("Parent: {}", parent.getId());

    model->m_isInScene = true;
  }

  if(isInstanced) {
    auto assetInstance = model->getAssetInstance();
    _filament->getFilamentScene()->addEntity(assetInstance->getRoot());
  }

  // TODO: (see `loadModelGlb`) move ECS setup here
  // NOTE: how do we get an assetInstance from a non-instanceable?
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
  if (percentComplete != 1.0f) {
    return;
  }

  // Get the collision system to add collidables
  auto collisionSystem =
  ecs->getSystem<CollisionSystem>(
      "updateAsyncAssetLoading");
  if (collisionSystem == nullptr) {
    spdlog::warn("Failed to get collision system when loading model");
    // TODO: should we throw an exception here?
  }

  for (const auto& [guid, model] : m_mapszoAssets) { // TODO: use a different list to check loading

    auto assetPath = model->szGetAssetPath();

    // mark as loaded.
    _assetsLoading.erase(assetPath);

    // once we're done loading our assets; we should be able to load instanced
    // models.
    if (auto foundAwaitingIter = _entitiesAwaitingLoadInstanced.find(assetPath);
        model->isInstanced() &&
        foundAwaitingIter != _entitiesAwaitingLoadInstanced.end()) {
      spdlog::info("Loading additional instanced assets: {}", assetPath);
      for (const auto& itemToLoad : foundAwaitingIter->second) {
        spdlog::info("Loading subset: {}", assetPath);
        std::vector<uint8_t> emptyVec;

        // retry loading the model, as we're done loading the instanceable data
        spdlog::debug("Calling loadModelGlb (instanceable: {}) after async load -> {}",
                      itemToLoad->isInstanced(), assetPath);

        // TODO: this shouldn't be called here! but it's needed
        // Basically this function does two things:
        // - actually starts the async load from buffer (when called in `handleFile`)
        // - a bunch of other random things to set up instancing (no bueno! called here)
        // The latter is a big problem, because it causes non-instanceables to be initialized improperly,
        // and it relies on ECS insertion being done in Model::Deserialize, which SHOULDN'T HAPPEN!
        // ECS insertion needs to be moved elsewhere (see other TODOs) and the actual instancing
        // should be done elsewhere, not in `loadModelGlb`.
        // Maybe a `setupInstancedModel()`, but that requires on the 'instanceableness' of the entity
        // being tracked properly, which it's not.
        loadModelGlb(itemToLoad, emptyVec);
      }
      spdlog::info("Done Loading additional instanced assets: {}", assetPath);
      _entitiesAwaitingLoadInstanced.erase(assetPath);
    }
    // else {
    //   spdlog::info("No instanced assets to load: {}", assetPath);
    // }

    // You don't get collision as a primary model instance.
    if (model->isInstancePrimary()) {
      continue;
    }

    // Insert models into the scene
    addModelToScene(model.get(), false);

    // if its 'done' loading, we need to create our large AABB collision
    // object if this model it's referencing required one.
    //
    // Also need to make sure it hasn't already created one for this model.

    // TODO: fix, this crashes
    // if (model->HasComponent(Component::StaticGetTypeID<Collidable>()) &&
    //     !collisionSystem->hasEntity(guid)) {
    //   collisionSystem->vAddCollidable(model.get());
    // }
  }
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> ModelSystem::loadGlbFromAsset(
    std::shared_ptr<Model> model,
    const std::string& path) {
  const auto promise(
      std::make_shared<std::promise<Resource<std::string_view>>>());
  auto promise_future(promise->get_future());

  try {
    const asio::io_context::strand& strand_(
        *ecs->GetStrand());

    const auto assetPath =
        ecs->getConfigValue<std::string>(kAssetPath);

    const bool bWantsInstancedData = model->isInstanced();
    const bool hasInstancedDataLoaded =
        m_mapInstanceableAssets_.find(model->szGetAssetPath()) !=
        m_mapInstanceableAssets_.end();
    const bool isCurrentlyLoadingInstanceableData =
        _assetsLoading.find(
            model->szGetAssetPath()) !=
        _assetsLoading.end();

    // if we are loading instanceable data...
    if (bWantsInstancedData) {
      std::string szAssetPath = model->szGetAssetPath();
      // ... and we're currently loading it or it's already loaded
      // add it to the list of assets to update when we're done loading.
      // see: updateAsyncAssetLoading() (which the calls this again)
      if (isCurrentlyLoadingInstanceableData || hasInstancedDataLoaded) {
        const auto iter =
            _entitiesAwaitingLoadInstanced.find(model->szGetAssetPath());

        // find/create a list of instances to load
        // and add this model to it.
        if (iter != _entitiesAwaitingLoadInstanced.end()) {
          iter->second.emplace_back(std::move(model));
        } else {
          std::list<std::shared_ptr<Model>> modelListToLoad;
          modelListToLoad.emplace_back(std::move(model));
          _entitiesAwaitingLoadInstanced.insert(
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
      _assetsLoading.insert(szAssetPath);
    }

    post(strand_,
         [&, model = std::move(model), promise, path, assetPath]() mutable {
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
void ModelSystem::handleFile(std::shared_ptr<Model>&& oOurModel,
                             const std::vector<uint8_t>& buffer,
                             const std::string& fileSource,
                             const PromisePtr& promise) {
  spdlog::debug("handleFile");
  if (!buffer.empty()) {
    spdlog::debug("Calling loadModelGlb (instanceable: {}) instantly -> {}", 
                  oOurModel->isInstanced(), fileSource);
    loadModelGlb(std::move(oOurModel), buffer);
    promise->set_value(Resource<std::string_view>::Success(
        "Loaded glb model successfully from " + fileSource));
  } else {
    promise->set_value(Resource<std::string_view>::Error(
        "Couldn't load glb model from " + fileSource));
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vOnInitSystem() {
  if (materialProvider_ != nullptr) {
    return;
  }

  // Get filament
  _filament = ecs->getSystem<FilamentSystem>(__FUNCTION__);
  runtime_assert(_filament != nullptr, "ModelSystem::vOnInitSystem: FilamentSystem not init yet");

  _engine = _filament->getFilamentEngine();
  runtime_assert(_engine != nullptr, "ModelSystem::vOnInitSystem: FilamentEngine not found");

  _rcm = _engine->getRenderableManager();
  _tm = _engine->getTransformManager();
  _em = _engine->getEntityManager();

  runtime_assert(_rcm != nullptr, "ModelSystem::vOnInitSystem: RenderableManager not found");
  runtime_assert(_tm != nullptr, "ModelSystem::vOnInitSystem: TransformManager not found");
  runtime_assert(_em != nullptr, "ModelSystem::vOnInitSystem: EntityManager not found");

  spdlog::debug("[{}] loaded filament systems", __FUNCTION__);

  materialProvider_ = filament::gltfio::createUbershaderProvider(
      _engine, UBERARCHIVE_DEFAULT_DATA,
      static_cast<size_t>(UBERARCHIVE_DEFAULT_SIZE));

      // new NameComponentManager(EntityManager::get());
  names_ = new ::utils::NameComponentManager(::utils::EntityManager::get());

  SPDLOG_DEBUG("UbershaderProvider MaterialsCount: {}",
               materialProvider_->getMaterialsCount());

  AssetConfiguration assetConfiguration{};
  assetConfiguration.engine = _engine;
  assetConfiguration.materials = materialProvider_;
  assetConfiguration.names = names_;
  assetLoader_ = AssetLoader::create(assetConfiguration);

  ResourceConfiguration resourceConfiguration{};
  resourceConfiguration.engine = _engine;
  resourceConfiguration.normalizeSkinningWeights = true;
  resourceLoader_ = new ResourceLoader(resourceConfiguration);

  const auto decoder = filament::gltfio::createStbProvider(_engine);
  resourceLoader_->addTextureProvider("image/png", decoder);
  resourceLoader_->addTextureProvider("image/jpeg", decoder);
  // TODO: add support for other texture formats here

  // ChangeTranslationByGUID
  // TODO: move to TransformSystem
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
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetPosition(position);
          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);
        }

        SPDLOG_TRACE("ChangeTranslationByGUID Complete");
      });

  // ChangeRotationByGUID
  // TODO: move to TransformSystem
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
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetRotation(rotation);
          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);
        }

        SPDLOG_TRACE("ChangeRotationByGUID Complete");
      });

  // ChangeScaleByGUID
  // TODO: move to TransformSystem
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
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          transform->SetScale(values);
          EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *transform);
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

          if (const auto modelAsset = ourEntity->second->getAsset()) {
            if (value) {
              _filament->getFilamentScene()->addEntities(
                  modelAsset->getRenderableEntities(),
                  modelAsset->getRenderableEntityCount());
            } else {
              _filament->getFilamentScene()->removeEntities(
                  modelAsset->getRenderableEntities(),
                  modelAsset->getRenderableEntityCount());
            }
          } else if (const auto modelAssetInstance =
                         ourEntity->second->getAssetInstance()) {
            if (value) {
              _filament->getFilamentScene()->addEntities(
                  modelAssetInstance->getEntities(),
                  modelAssetInstance->getEntityCount());
            } else {
              _filament->getFilamentScene()->removeEntities(
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
  if (model->HasComponent(Component::StaticGetTypeID<Collidable>()) &&
      collisionSystem->hasEntity(guid)) {
    collisionSystem->vRemoveCollidable(model.get());
    collisionSystem->vAddCollidable(model.get());
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vUpdate(float /*fElapsedTime*/) {
  // Make sure all models are loaded
  updateAsyncAssetLoading();

  // Instance models that have just been loaded
  // TODO: see TODOs in `updateAsyncAssetLoading`
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
