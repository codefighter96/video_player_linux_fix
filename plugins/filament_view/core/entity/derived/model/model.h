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

#include <core/components/derived/animation.h>
#include <core/components/derived/basetransform.h>
#include <core/components/derived/commonrenderable.h>
#include <core/entity/base/entityobject.h>
#include <core/entity/derived/renderable_entityobject.h>
#include <gltfio/FilamentAsset.h>
#include <string>

namespace plugin_filament_view {

class Model : public RenderableEntityObject {
  private:
    /// @brief JSON data configuring the model.
    /// This is used to set up the model's properties and behavior.
    /// It is passed to the constructor and stored as a member variable,
    /// and deallocated following the model's initialization.
    smarter_raw_ptr<const flutter::EncodableMap> _initParams;
 public:
  Model(std::string assetPath,
        const flutter::EncodableMap& params);

  ~Model() override = default;

  static std::shared_ptr<Model> Deserialize(
      const std::string& flutterAssetsPath,
      const flutter::EncodableMap& params);

  // Disallow copy and assign.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  void setAsset(filament::gltfio::FilamentAsset* poAsset) {
    m_poAsset = poAsset;
  }

  void setAssetInstance(filament::gltfio::FilamentInstance* poAssetInstance) {
    m_poAssetInstance = poAssetInstance;
  }

  void SetPrimaryAssetToInstanceFrom(bool bValue) {
    m_bIsPrimaryAssetToInstanceFrom = bValue;
  }

  [[nodiscard]] filament::gltfio::FilamentAsset* getAsset() const {
    return m_poAsset;
  }

  [[nodiscard]] filament::gltfio::FilamentInstance* getAssetInstance() const {
    return m_poAssetInstance;
  }

  [[nodiscard]] std::shared_ptr<BaseTransform> GetBaseTransform() const {
    return GetComponent<BaseTransform>();
  }
  [[nodiscard]] std::shared_ptr<CommonRenderable> GetCommonRenderable() const {
    return GetComponent<CommonRenderable>();
  }

  [[nodiscard]] std::string szGetAssetPath() const { return assetPath_; }
  [[nodiscard]] std::string szGetURLPath() const { return url_; }

  [[nodiscard]] bool bShouldKeepAssetDataInMemory() const {
    return m_bShouldKeepAssetDataInMemory;
  }

  [[nodiscard]] bool bIsPrimaryAssetToInstanceFrom() const {
    return m_bIsPrimaryAssetToInstanceFrom;
  }
  [[nodiscard]] filament::Aabb poGetBoundingBox() {
    if (m_poAsset != nullptr) {
      return m_poAsset->getBoundingBox();
    } else if (m_poAssetInstance != nullptr) {
      return m_poAssetInstance->getBoundingBox();
    }

    return {};
  }

 protected:
  std::string assetPath_;
  std::string url_;

  filament::gltfio::FilamentAsset* m_poAsset;
  filament::gltfio::FilamentInstance* m_poAssetInstance;

  // used for instancing objects.
  bool m_bShouldKeepAssetDataInMemory = false;
  bool m_bIsPrimaryAssetToInstanceFrom = false;

  void DebugPrint() const override;

  void onInitialize() override;

  /// material to be used for the model - instantiated from material definition
  /// Only after a run time request to change has been made.
  /// This should probably be on the entity level as model would use this in
  /// future as well.
  Resource<filament::MaterialInstance*> m_poMaterialInstance;
  void vLoadMaterialDefinitionsToMaterialInstance();

  void vChangeMaterialDefinitions(
      const flutter::EncodableMap& /*params*/,
      const TextureMap& /*loadedTextures*/) override;
  void vChangeMaterialInstanceProperty(
      const MaterialParameter* /*materialParam*/,
      const TextureMap& /*loadedTextures*/) override;
};

class GlbModel final : public Model {
 public:
  GlbModel(std::string assetPath,
           const flutter::EncodableMap& params);

  ~GlbModel() override = default;
};
}  // namespace plugin_filament_view
