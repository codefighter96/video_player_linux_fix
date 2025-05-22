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
#pragma once

#include <core/entity/derived/model/model.h>
#include <core/include/resource.h>
#include <core/systems/base/ecsystem.h>
#include <core/systems/derived/filament_system.h>
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <filament/utils/NameComponentManager.h>
#include <filament/filament/RenderableManager.h>
#include <filament/TransformManager.h>
#include <asio/io_context_strand.hpp>
#include <future>
#include <list>
#include <set>

namespace plugin_filament_view {

enum class AssetLoadingState {
  /// Default value, not set
  unset = 0,
  /// Used when a model is queued for loading
  loading,
  /// Used when a model is loaded and is ready to be used
  loaded,
  /// Used when a model failed to load
  error = -1
};

/// @brief Holds the state of an asset being loaded
/// It contains the path to the asset, the loading state,
/// and a list of [Model]s that are waiting for this asset to be loaded
struct AssetDescriptor {
  /// assetPath
  // std::string* path = nullptr;
  /// asset loading state
  AssetLoadingState state = AssetLoadingState::unset;
  /// list of models that are waiting for this asset to be loaded
  /// once the asset is loaded, this list will be used to load the models
  /// and then clear the list
  std::list<EntityGUID> loadingInstances = {};
  filament::gltfio::FilamentAsset* asset = nullptr;
};

class Model;
class TransformSystem;

class ModelSystem : public ECSystem {
 public:
  ModelSystem() = default;

  void destroyAllAssetsOnModels();
  void destroyAsset(const filament::gltfio::FilamentAsset* asset) const;

  void createModelInstance(Model* model);

  void queueModelLoad(
    std::shared_ptr<Model> oOurModel
  );

  void vOnInitSystem() override;
  void vUpdate(float fElapsedTime) override;
  void vShutdownSystem() override;
  void DebugPrint() override;

 private:
  // update steps
  /// Every frame, update the status of asset loading
  /// and instantiate them where necessary
  void updateAsyncAssetLoading();
  /// Every frame, make sure that models with loaded assets are added to the scene
  void updateModelsInScene();

 private:
  ::filament::gltfio::AssetLoader* assetLoader_{};
  ::filament::gltfio::MaterialProvider* materialProvider_{};
  ::filament::gltfio::ResourceLoader* resourceLoader_{};
  ::utils::NameComponentManager* names_{};

  // filamentEngine, RenderableManager, EntityManager, TransformManager
  smarter_shared_ptr<TransformSystem> _transforms;
  smarter_shared_ptr<FilamentSystem> _filament;
  smarter_raw_ptr<filament::Engine> _engine;
  smarter_raw_ptr<filament::RenderableManager> _rcm;
  smarter_raw_ptr<utils::EntityManager> _em;
  smarter_raw_ptr<filament::TransformManager> _tm;

  /// Map of asset paths to their loading states
  std::map<std::string, AssetDescriptor> _assets {};
  std::map<EntityGUID, std::shared_ptr<Model>> _models {};

  /// Creates renderables and adds them to the scene
  /// Expects the model to have been loaded first
  void addModelToScene(EntityGUID modelGuid);

  void setupRenderable(
    const FilamentEntity entity,
    Model* model,
    filament::gltfio::FilamentAsset* asset
  );

  /// Asynchronously loads a model from a file and returns a future
  /// that will be set when the model is loaded
  /// Expects model to have been queued first, via [ModelSystem::queueModelLoad]
  void loadModelFromFile(
    EntityGUID modelGuid,
    const std::string& baseAssetPath
  );

};
}  // namespace plugin_filament_view
