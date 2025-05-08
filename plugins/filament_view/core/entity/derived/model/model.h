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
  friend class ModelSystem;
 public:
  Model();

  ~Model() override = default;

  /// @brief Static deserializer - calls the constructor and deserializeFrom under the hood
  static std::shared_ptr<Model> Deserialize(const flutter::EncodableMap& params);

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
    m_isInstancePrimary = bValue;
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

  /// Returns whether the model is a secondary instance of a model asset
  [[nodiscard]] bool isInstanced() const {
    return m_isInstanced;
  }

  /// Returns whether the model is a primary instanceable asset
  [[nodiscard]] bool isInstancePrimary() const {
    return m_isInstancePrimary;
  }

  /// Returns whether the model is in the scene
  [[nodiscard]] bool isInScene() const {
    return m_isInScene;
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

  filament::gltfio::FilamentAsset* m_poAsset;
  filament::gltfio::FilamentInstance* m_poAssetInstance;

  /// Whether the model is a secondary instance of a model asset
  bool m_isInstanced = false;
  /// Whether the model is a primary instanceable asset
  bool m_isInstancePrimary = false;
  /// Whether it's been inserted into the scene
  bool m_isInScene = false;

  void DebugPrint() const override;

  void onInitialize() override;

  virtual void deserializeFrom(const flutter::EncodableMap& params) override;


  void vChangeMaterialDefinitions(
      const flutter::EncodableMap& /*params*/,
      const TextureMap& /*loadedTextures*/) override;
  void vChangeMaterialInstanceProperty(
      const MaterialParameter* /*materialParam*/,
      const TextureMap& /*loadedTextures*/) override;

 private:
  /*
   *  Init data (temporary)
   */
  std::shared_ptr<Animation> _tmpAnimation = nullptr;
};

}  // namespace plugin_filament_view
