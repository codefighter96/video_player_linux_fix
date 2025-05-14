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
  for (const auto& [fst, snd] : _models) {
    destroyAsset(snd->getAsset());  // NOLINT
  }
  _models.clear();
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
  const auto iter = _models.find(guid);
  if (iter == _models.end()) {
    return nullptr;
  }

  return iter->second->getAsset();
}

bool ModelSystem::setupRenderable(
  const FilamentEntity entity,
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
  const FilamentEntity entity,
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
      childEntity->_fEntity = _em->create();

      spdlog::debug("Created entityobject");

      const int rootEntityId = 0;
      // filament::math::mat4f localMatrix = filament::math::mat4f();
      FilamentEntity en = childEntity->_fEntity;
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
        childEntity->_fEntity,
        transform->GetRotation(),
        transform->GetScale(),
        transform->GetPosition(),
        &(model->_fEntity)
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

      // TODO: should this be done here?
      // collisionSystem->vAddCollidable(childEntity.get());

      spdlog::debug("Collidable added!");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::createModelInstance(
  Model* model
) {
  checkInitialized();
  debug_assert(
    assetLoader_ != nullptr,
    "ModelSystem::createModelInstance - assetLoader_ is null"
  );

  const auto assetPath = model->getAssetPath();
  spdlog::debug("\nModelSystem::createModelInstance: {}", assetPath);
  spdlog::debug("instance mode: {}",
                modelInstancingModeToString(model->getInstancingMode()));


  // NOTE: if you're creating a lot of instances, this is better to use at the
  // start FilamentAsset* createInstancedAsset(const uint8_t* bytes, uint32_t
  // numBytes, FilamentInstance **instances, size_t numInstances)
  /// NOTE: is it tho?
  filament::gltfio::FilamentAsset* asset = nullptr;
  filament::gltfio::FilamentInstance* assetInstance = nullptr;

  std::vector<Entity> collidableChildren = {};

  // check if instanceable (primary) is loaded
  const auto instancedModelData = _assets[assetPath];
  if (instancedModelData.state != AssetLoadingState::unset) {
    spdlog::debug("Primary confirmed loaded");
    asset = instancedModelData.asset;
    runtime_assert(
      asset != nullptr,
      "ModelSystem::createModelInstance - asset CANNOT be null"
    );

    // if the asset is not instanceable, it will return nullptr
    assetInstance = assetLoader_->createInstance(asset);
    spdlog::debug("Asset instance created!");
    // if not nullptr, it means it's a valid secondary instance
  } else {
    throw std::runtime_error(
      fmt::format("ModelSystem::createModelInstance - asset {} not loaded", assetPath)
    );
  }

  // instance / secondary object.
  // asset loaded, 
  spdlog::debug("HAS INSTANCE");
  model->setAssetInstance(assetInstance);

  // by this point we should either have a subinstance, or THE primary instance
  runtime_assert(assetInstance != nullptr, "This should NEVER be null!");
}

void ModelSystem::addModelToScene(
  EntityGUID modelGuid
) {
  checkInitialized();

  // Get model
  std::shared_ptr<Model> model = _models[modelGuid];
  runtime_assert(
    model != nullptr,
    fmt::format(
      "[{}] Can't add model({}) to scene, model is null",
      __FUNCTION__, modelGuid
    ).c_str()
  );

  // Expects the model to be already loaded
  runtime_assert(
    _assets[model->getAssetPath()].state == AssetLoadingState::loaded,
    fmt::format(
      "[{}] Can't add model({}) to scene, asset not loaded (asset state: {})",
      __FUNCTION__, modelGuid, modelInstancingModeToString(model->getInstancingMode())
    ).c_str()
  );

  const bool isInScene = model->isInScene();
  const ModelInstancingMode instancingMode = model->getInstancingMode();

  if(isInScene) {
    spdlog::warn(
      "[{}] model '{}'({}) is already in scene (asset {}), skipping add",
      __FUNCTION__, model->GetName(), modelGuid, model->assetPath_
    );
    return;
  }

  FilamentEntity* modelEntities = nullptr;
  size_t modelEntityCount = 0;

  // Get the asset instance
  const auto asset = model->getAsset();
  const auto assetInstance = model->getAssetInstance();

  if(instancingMode == ModelInstancingMode::secondary) {
    // If it's a secondary instance, we need to get the entities from the asset instance
    runtime_assert(
      assetInstance != nullptr,
      "ModelSystem::addModelToScene: model asset instance cannot be null"
    );
    // runtime_assert(
    //   asset != nullptr,
    //   "ModelSystem::addModelToScene: model asset cannot be null"
    // );

    modelEntities = const_cast<FilamentEntity*>(assetInstance->getEntities());
    modelEntityCount = assetInstance->getEntityCount();
  } else {
    /// If it's non-instanced, we need to get the entities from the asset`
    // runtime_assert(
    //   asset != nullptr,
    //   "ModelSystem::addModelToScene: model asset cannot be null"
    // );

    if(asset == nullptr) {
      spdlog::warn(
        "[{}] model({}) asset({}) is null, deferring load till later (are we tho?)",
        __FUNCTION__, modelGuid, model->getAssetPath()
      );
      return;
    }

    modelEntities = const_cast<FilamentEntity*>(asset->getRenderableEntities());
    modelEntityCount = asset->getRenderableEntityCount();
  }

  /*
   *  Renderable setup
   */
  spdlog::debug("  Setting up renderables...");

  utils::Slice const renderables{
    modelEntities,
    modelEntityCount
  };

  if(instancingMode == ModelInstancingMode::primary) {
    spdlog::debug("  Model({}) is primary, not adding to scene", modelGuid);
    return;
  }

  // Add to ECS
  spdlog::debug("[{}] Adding model({}) to ECS", __FUNCTION__, modelGuid);
  ecs->addEntity(model);
  
  for (const auto entity : renderables) {
    // const auto entity = modelEntities[i];
    _filament->getFilamentScene()->addEntity(entity);

    // TODO: move this elsewhere to enable creation of proper children
    // const bool hasCollidableChild = 
      setupRenderable(
        entity,
        model.get(),
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

  // Set up collidable children
  // for (const auto entity : collidableChildren) {
  //   setupCollidableChild(entity, sharedPtr.get(), asset);
  // }

  spdlog::debug("  Adding model({}) to Filament scene", modelGuid);
  auto instanceEntity = assetInstance->getRoot();
  _filament->getFilamentScene()->addEntity(instanceEntity);

  // Set up transform
  EntityTransforms::vApplyTransform(instanceEntity, *model->GetBaseTransform());

  // Set up renderable
  /// TODO: set up renderable (add _fEntity to model, etc)

  // Set up collidable
  /// TODO: set up collidable

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
    // animationPtr->DebugPrint("From ModelSystem::addModelToScene\t");
  } else if (animatorInstance != nullptr &&
             animatorInstance->getAnimationCount() > 0) {
    SPDLOG_DEBUG(
        "For asset - {} you have a valid set of animations [{}] you can play "
        "on this, but you didn't load an animation component, load one if you "
        "want that "
        "functionality",
        model->getAssetPath(), animatorInstance->getAnimationCount());
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::updateAsyncAssetLoading() {
  // spdlog::debug(
  //   "[{}] Updating async asset loading",
  //   __FUNCTION__
  // );

  // This does not specify per resource, but a global, best we can do with this
  // information is if we're done loading <everything> that was marked as async
  // load, then load that physics data onto a collidable if required. This gives
  // us visuals without collidables in a scene with <tons> of objects; but would
  // eventually settle
  resourceLoader_->asyncUpdateLoad();
  const float percentComplete = resourceLoader_->asyncGetLoadProgress();
  if (percentComplete != 1.0f) {
    spdlog::debug(
      "[{}] Model async loading progress: {}%",
      __FUNCTION__, percentComplete * 100.0f
    );
    return;
  } else {
    // spdlog::info(
    //   "[{}] Async model loading complete",
    //   __FUNCTION__
    // );
  }

  // for each AssetDescriptor...
  for (auto& [assetPath, assetData] : _assets) {
    // if the asset is loaded, mark it as loaded
    if (assetData.state == AssetLoadingState::loading) {
      assetData.state = AssetLoadingState::loaded;
    }

    if(assetData.state == AssetLoadingState::loaded) {
      // if the asset is loaded, load the models
      for (auto& modelGuid : assetData.loadingInstances) {
        // get the model
        std::shared_ptr<Model> model = _models[modelGuid];
        if (model == nullptr) {
          spdlog::error(
            "[{}] Model {} not found",
            __FUNCTION__, modelGuid
          );
          continue;
        }

        if(model->isInScene()) {
          spdlog::warn("Model {} is already in scene, skipping load", model->GetName());
          continue;
        }

        // load the model
        spdlog::debug("Loading model: {}", model->getAssetPath());

        switch(model->getInstancingMode()) {
          case ModelInstancingMode::primary:
            spdlog::debug("Model is primary, updating transform but not adding to scene");
            // TODO: the below line is temporary, it should be done as a part of addModelToScene
            // EntityTransforms::vApplyTransform(model->getAssetInstance()->getRoot(), *model->_tmpTransform);
            break;
          case ModelInstancingMode::secondary:
            // load the model as an instance
            spdlog::debug("Loading model as instance: {}", model->getAssetPath());
            createModelInstance(model.get());
            spdlog::debug("Model instanced, adding to scene...");
            addModelToScene(modelGuid);
            spdlog::debug("Model added to scene! Yay!");

            // Set up collidable children
            // for (const auto entity : collidableChildren) {
            //   setupCollidableChild(entity, sharedPtr.get(), asset);
            // }
            break;
          case ModelInstancingMode::none:
            // load the model as a single object
            spdlog::debug("Loading model as single object: {}", model->getAssetPath());
            addModelToScene(modelGuid);
            break;
        }
      }

      // clear the loading instances
      assetData.loadingInstances.clear();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::queueModelLoad(
  std::shared_ptr<Model> model
) {
  spdlog::debug("Queueing model({}) load (instance mode: {}) -> {}", 
    model->GetGuid(),
    modelInstancingModeToString(model->getInstancingMode()),
    model->getAssetPath()
  );

  try {
    const auto baseAssetPath = ecs->getConfigValue<std::string>(kAssetPath);
    const auto modelAssetPath = model->getAssetPath();
    const EntityGUID modelGuid = model->GetGuid();
    const ModelInstancingMode instanceMode = model->getInstancingMode();

    AssetDescriptor& assetData = _assets[modelAssetPath];

    switch(assetData.state) {
      /// Unset: not yet in queue
      case AssetLoadingState::unset:
        // mark as loading, add asset
        // assetData.path = &modelAssetPath;
        assetData.state = AssetLoadingState::loading;
        _models[modelGuid] = std::move(model);
        assetData.loadingInstances.emplace_back(modelGuid);

        spdlog::debug("Asset unset: queued for loading.");
        loadModelFromFile(
          modelGuid,
          baseAssetPath
        );
        return;
      /// Loading: asset already in queue
      case AssetLoadingState::loading:
        // add model to asset's loading queue
        _models[modelGuid] = model;
        if(instanceMode == ModelInstancingMode::primary) {
          spdlog::warn("Double-load of primary model({}): {}",
            modelGuid,
            modelAssetPath
          );
        } else {
          assetData.loadingInstances.emplace_back(modelGuid);
        }
        spdlog::debug("Asset loading: model queued for loading.");
        return;
      /// Loaded: asset in memory, can instance
      case AssetLoadingState::loaded:
        // add model to asset's loading queue
        _models[modelGuid] = std::move(model);
        assetData.loadingInstances.emplace_back(modelGuid);
        spdlog::debug("Asset loaded: model queued for instancing.");
        return;
      /// Error: asset failed to load
      case AssetLoadingState::error:
        /// TODO: make sure this state is being set to begin with
        spdlog::error(
          "[ModelSystem::queueModelLoad] Asset {} failed to load, cannot queue model({})",
          modelAssetPath, modelGuid
        );
        /// TODO: actuallly handle the error somehow?
        break;
    }
  } catch (const std::exception& e) {
    throw std::runtime_error(
      "[ModelSystem::queueModelLoad] Failed to queue model load: " + std::string(e.what())
    );
  }
}

void ModelSystem::loadModelFromFile(
  EntityGUID modelGuid,
  const std::string baseAssetPath
) {
  spdlog::debug("++ loadModelFromFile");

  const auto& strand = *ecs->GetStrand();
  post(strand, [&, modelGuid, baseAssetPath]() mutable {
    spdlog::debug("++ loadModelFromFile (lambda), model guid: {}", modelGuid);
    // Get model
    std::shared_ptr<Model> model = _models[modelGuid];
    if (model == nullptr) {
      throw std::runtime_error(
        fmt::format("[loadModelFromFile] Model {} not found", modelGuid)
      );
    }

    try {
      const auto assetPath = model->getAssetPath();
      spdlog::debug("Loading model from assetPath: {}", assetPath);

      // Read the file and handle buffer
      const auto buffer = readBinaryFile(assetPath, baseAssetPath);
      spdlog::debug("handleFile");
      if (!buffer.empty()) {
        // Load GLB asset

        // Note if you're creating a lot of instances, this is better to use at the
        // start FilamentAsset* createInstancedAsset(const uint8_t* bytes, uint32_t
        // numBytes, FilamentInstance **instances, size_t numInstances)
        filament::gltfio::FilamentAsset* asset = nullptr;
        filament::gltfio::FilamentInstance* assetInstance = nullptr;


        asset = assetLoader_->createAsset(
          buffer.data(),
          static_cast<uint32_t>(buffer.size())
        );
        spdlog::debug("[loadModelFromFile] asyncBeginLoad");
        resourceLoader_->asyncBeginLoad(asset);
        model->setAsset(asset);
        _assets[assetPath].asset = asset; // important! if not set, secondaries cannot be created

        // release source data
        if(model->getInstancingMode() == ModelInstancingMode::none) {
          spdlog::debug("[loadModelFromFile] Non-secondary loaded: releasing source data");
          asset->releaseSourceData(); // TODO: do we also call this for primaries after instancing?
        }

        assetInstance = asset->getInstance();
        runtime_assert(
          assetInstance != nullptr,
          "[loadModelFromFile] Failed to fetch primary asset instance"
        );

        model->setAssetInstance(assetInstance);
    
        spdlog::info("Loaded glb model successfully from {}", assetPath);
      } else {
        spdlog::error("Couldn't load glb model from {}", assetPath);
      }
      
    } catch (const std::exception& e) {
      spdlog::error("[{}] Failed to load: {}", __FUNCTION__, e.what());
    } catch (...) {
      spdlog::error("[{}] Unknown Exception in lambda", __FUNCTION__);
    }
  });
}


////////////////////////////////////////////////////////////////////////////////////
void ModelSystem::vOnInitSystem() {
  spdlog::debug("[{}] Initializing ModelSystem", __FUNCTION__);
  // TODO: can be removed pending LifecycleParticipant impl.
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
        if (const auto ourEntity = _models.find(guid);
            ourEntity != _models.end()) {
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
        if (const auto ourEntity = _models.find(guid);
            ourEntity != _models.end()) {
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
        if (const auto ourEntity = _models.find(guid);
            ourEntity != _models.end()) {
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

        if (const auto ourEntity = _models.find(guid);
            ourEntity != _models.end()) {

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
