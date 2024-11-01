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

#include <core/components/derived/basetransform.h>
#include <core/components/derived/commonrenderable.h>
#include <core/entity/base/entityobject.h>
#include <core/entity/derived/model/animation/animation.h>
#include <gltfio/FilamentAsset.h>
#include <string>

namespace plugin_filament_view {

class Animation;

class Model : public EntityObject {
 public:
  Model(std::string assetPath,
        std::string url,
        Model* fallback,
        Animation* animation,
        std::shared_ptr<BaseTransform> poTransform,
        std::shared_ptr<CommonRenderable> poCommonRenderable,
        const flutter::EncodableMap& params);

  ~Model() override = default;

  static std::unique_ptr<Model> Deserialize(
      const std::string& flutterAssetsPath,
      const flutter::EncodableMap& params);

  [[nodiscard]] Model* GetFallback() const { return fallback_; }

  [[nodiscard]] Animation* GetAnimation() const { return animation_; }

  // Disallow copy and assign.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  void setAsset(filament::gltfio::FilamentAsset* poAsset) {
    m_poAsset = poAsset;
  }

  [[nodiscard]] filament::gltfio::FilamentAsset* getAsset() const {
    return m_poAsset;
  }

  [[nodiscard]] std::shared_ptr<BaseTransform> GetBaseTransform() const {
    return m_poBaseTransform.lock();
  }
  [[nodiscard]] std::shared_ptr<CommonRenderable> GetCommonRenderable() const {
    return m_poCommonRenderable.lock();
  }

  [[nodiscard]] std::string szGetAssetPath() const { return assetPath_; }
  [[nodiscard]] std::string szGetURLPath() const { return url_; }

 protected:
  std::string assetPath_;
  std::string url_;
  Model* fallback_;
  Animation* animation_;

  filament::gltfio::FilamentAsset* m_poAsset;

  void DebugPrint() const override;

  // Components - saved off here for faster
  // lookup, but they're not owned here, but on EntityObject's list.
  std::weak_ptr<BaseTransform> m_poBaseTransform;
  std::weak_ptr<CommonRenderable> m_poCommonRenderable;

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
           std::string url,
           Model* fallback,
           Animation* animation,
           std::shared_ptr<BaseTransform> poTransform,
           std::shared_ptr<CommonRenderable> poCommonRenderable,
           const flutter::EncodableMap& params);

  ~GlbModel() override = default;

  friend class ModelManager;
  friend class SceneController;
};

class GltfModel final : public Model {
 public:
  GltfModel(std::string assetPath,
            std::string url,
            std::string pathPrefix,
            std::string pathPostfix,
            Model* fallback,
            Animation* animation,
            std::shared_ptr<BaseTransform> poTransform,
            std::shared_ptr<CommonRenderable> poCommonRenderable,
            const flutter::EncodableMap& params);

  ~GltfModel() override = default;

  friend class ModelManager;
  friend class SceneController;

  [[nodiscard]] std::string szGetPrefix() const { return pathPrefix_; }
  [[nodiscard]] std::string szGetPostfix() const { return pathPostfix_; }

 private:
  std::string pathPrefix_;
  std::string pathPostfix_;
};

}  // namespace plugin_filament_view
