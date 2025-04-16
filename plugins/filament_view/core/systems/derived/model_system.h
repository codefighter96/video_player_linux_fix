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
#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <filament/utils/NameComponentManager.h>
#include <filament/filament/RenderableManager.h>
#include <asio/io_context_strand.hpp>
#include <future>
#include <list>

namespace plugin_filament_view {

class Model;

class ModelSystem : public ECSystem {
 public:
  ModelSystem() = default;

  void destroyAllAssetsOnModels();
  void destroyAsset(const filament::gltfio::FilamentAsset* asset) const;

  void loadModelGlb(std::shared_ptr<Model> oOurModel,
                    const std::vector<uint8_t>& buffer,
                    const std::string& assetName);

  void loadModelGltf(std::shared_ptr<Model> oOurModel,
                     const std::vector<uint8_t>& buffer,
                     std::function<const ::filament::backend::BufferDescriptor&(
                         std::string uri)>& callback);

  filament::gltfio::FilamentAsset* poFindAssetByGuid(const EntityGUID guid);

  void updateAsyncAssetLoading();

  /// Returns whether has extras
  bool setupRenderable(
      const Entity entity,
      const Model* model,
      filament::gltfio::FilamentAsset* asset,
      filament::RenderableManager& rcm,
      utils::EntityManager& em);

  void setupCollidableChild(
    const Entity entity,
    const Model* model,
    filament::gltfio::FilamentAsset* asset,
    filament::RenderableManager& rcm,
    utils::EntityManager& em,
    filament::TransformManager& tm
  );

  std::future<Resource<std::string_view>> loadGlbFromAsset(
      std::shared_ptr<Model> oOurModel,
      const std::string& path);

  std::future<Resource<std::string_view>> loadGlbFromUrl(
      std::shared_ptr<Model> oOurModel,
      std::string url);

  static std::future<Resource<std::string_view>> loadGltfFromAsset(
      const std::shared_ptr<Model>& oOurModel,
      const std::string& path,
      const std::string& pre_path,
      const std::string& post_path);

  static std::future<Resource<std::string_view>> loadGltfFromUrl(
      const std::shared_ptr<Model>& oOurModel,
      const std::string& url);

  void vOnInitSystem() override;
  void vUpdate(float fElapsedTime) override;
  void vShutdownSystem() override;
  void DebugPrint() override;

 private:
  ::filament::gltfio::AssetLoader* assetLoader_{};
  ::filament::gltfio::MaterialProvider* materialProvider_{};
  ::filament::gltfio::ResourceLoader* resourceLoader_{};
  ::utils::NameComponentManager* names_{};

  // This is the EntityObject guids to model instantiated.
  std::map<EntityGUID, std::shared_ptr<Model>> m_mapszoAssets;  // NOLINT

  // This will be needed for a list of prefab instances to load from
  std::map<std::string, filament::gltfio::FilamentAsset*>
      m_mapInstanceableAssets_;

  // So we start the program, and say we want 20 foxes. We only load '1',
  // queue the other ones in here, and instance off the main one when its
  // completed.
  std::map<std::string, std::list<std::shared_ptr<Model>>>
      m_mapszoAssetsAwaitingDataLoad;

  // When loading, it will be in here so we know not to load more than 1
  /// Map of asset paths to whether they are currently being loaded.
  std::map<std::string, bool> m_mapszbCurrentlyLoadingInstanceableAssets;

  // This is a reusable list of renderables for popping off
  // async load.
  // NOTE If you change this size; the async update loop on the system count
  // needs to change.
  utils::Entity readyRenderables_[128];

  // not actively used, to be moved
  std::vector<float> morphWeights_;

  void vSetupAssetThroughoutECS(
      std::shared_ptr<Model>& sharedPtr,
      filament::gltfio::FilamentAsset* filamentAsset,
      filament::gltfio::FilamentInstance* filamentAssetInstance);

  void populateSceneWithAsyncLoadedAssets(const Model* model);

  void vRemoveAndReaddModelToCollisionSystem(
      const EntityGUID guid,
      const std::shared_ptr<Model>& model);

  using PromisePtr = std::shared_ptr<std::promise<Resource<std::string_view>>>;
  void handleFile(
      std::shared_ptr<Model>&& oOurModel,
      const std::vector<uint8_t>& buffer,
      const std::string& fileSource,
      const PromisePtr&
          promise);  // NOLINT(readability-avoid-const-params-in-decls)
};
}  // namespace plugin_filament_view
