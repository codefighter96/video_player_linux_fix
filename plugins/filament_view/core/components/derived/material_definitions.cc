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

#include "material_definitions.h"

#include <core/include/literals.h>
#include <core/systems/ecs.h>
#include <filament/Material.h>
#include <filament/TextureSampler.h>
#include <plugins/common/common.h>
#include <filesystem>

namespace plugin_filament_view {

using MinFilter = filament::TextureSampler::MinFilter;
using MagFilter = filament::TextureSampler::MagFilter;

///////////////////////////////////////////////////////////////////////////////////////////////////
MaterialDefinitions::MaterialDefinitions(const flutter::EncodableMap& params)
    : Component(std::string(__FUNCTION__)) {
  SPDLOG_TRACE("++{}", __FUNCTION__);
  const auto flutterAssetPath =
      ECSManager::GetInstance()->getConfigValue<std::string>(kAssetPath);

  for (const auto& [fst, snd] : params) {
    auto key = std::get<std::string>(fst);
    SPDLOG_TRACE("Material Param {}", key);

    if (snd.IsNull() && key != "url") {
      SPDLOG_WARN("Material Param Second mapping is null {}", key);
      continue;
    }

    if (snd.IsNull() && key == "url") {
      SPDLOG_TRACE("Material Param URL mapping is null {}", key);
      continue;
    }

    if (key == "assetPath" && std::holds_alternative<std::string>(snd)) {
      assetPath_ = std::get<std::string>(snd);
    } else if (key == "url" && std::holds_alternative<std::string>(snd)) {
      url_ = std::get<std::string>(snd);
    } else if (key == "parameters" &&
               std::holds_alternative<flutter::EncodableList>(snd)) {
      auto list = std::get<flutter::EncodableList>(snd);
      for (const auto& it_ : list) {
        auto parameter = MaterialParameter::Deserialize(
            flutterAssetPath, std::get<flutter::EncodableMap>(it_));
        parameters_.insert(
            std::pair(parameter->szGetParameterName(), std::move(parameter)));
      }
    } else if (!snd.IsNull()) {
      spdlog::debug("[Material] Unhandled Parameter {}", key.c_str());
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(), snd);
    }
  }
  SPDLOG_TRACE("--{}", __FUNCTION__);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
MaterialDefinitions::~MaterialDefinitions() {
  for (auto& [fst, snd] : parameters_) {
    snd.reset();
  }
  parameters_.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void MaterialDefinitions::DebugPrint(const std::string& tabPrefix) const {
  spdlog::debug(tabPrefix + "++++++++ (MaterialDefinitions) ++++++++");
  if (!assetPath_.empty()) {
    spdlog::debug(tabPrefix + "assetPath: [{}]", assetPath_);

    const auto flutterAssetPath =
        ECSManager::GetInstance()->getConfigValue<std::string>(kAssetPath);

    const std::filesystem::path asset_folder(flutterAssetPath);
    spdlog::debug(tabPrefix + "asset_path {} valid",
                  exists(asset_folder / assetPath_) ? "is" : "is not");
  }
  if (!url_.empty()) {
    spdlog::debug(tabPrefix + "url: [{}]", url_);
  }
  spdlog::debug(tabPrefix + "ParamCount: [{}]", parameters_.size());

  for (const auto& [fst, snd] : parameters_) {
    if (snd != nullptr)
      snd->DebugPrint(std::string(tabPrefix + "parameter").c_str());
  }

  spdlog::debug("-------- (MaterialDefinitions) --------");
}

///////////////////////////////////////////////////////////////////////////////////////////////////
std::string MaterialDefinitions::szGetMaterialDefinitionLookupName() const {
  if (!assetPath_.empty()) {
    return assetPath_;
  }
  if (!url_.empty()) {
    return url_;
  }
  return "Unknown";
}

///////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<MaterialParameter*>
MaterialDefinitions::vecGetTextureMaterialParameters() const {
  std::vector<MaterialParameter*> returnVector;

  for (const auto& [fst, snd] : parameters_) {
    // Check if the type is TEXTURE
    if (snd->type_ == MaterialParameter::MaterialType::TEXTURE) {
      returnVector.push_back(snd.get());
    }
  }

  return returnVector;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void MaterialDefinitions::vApplyMaterialParameterToInstance(
    filament::MaterialInstance* materialInstance,
    const MaterialParameter* param,
    const TextureMap& loadedTextures) {
  const std::string paramName = param->szGetParameterName();
  const char* szParamName = paramName.c_str();

  switch (param->type_) {
    case MaterialParameter::MaterialType::COLOR: {
      materialInstance->setParameter(szParamName, filament::RgbaType::LINEAR,
                                     param->colorValue_.value());
    } break;

    case MaterialParameter::MaterialType::FLOAT: {
      materialInstance->setParameter(szParamName, param->fValue_.value());
    } break;

    case MaterialParameter::MaterialType::TEXTURE: {
      // make sure we have the texture:
      const auto foundResource =
          loadedTextures.find(param->getTextureValueAssetPath());

      if (foundResource == loadedTextures.end()) {
        // log and continue
        spdlog::warn(
            "Got to a case where a texture was not loaded before trying to "
            "apply to a material.");
        return;
      }

      // sampler will be on 'our' deserialized
      // texturedefinitions->texture_sampler
      const auto textureSampler = param->getTextureSampler();

      filament::TextureSampler sampler(MinFilter::LINEAR, MagFilter::LINEAR);

      if (textureSampler != nullptr) {
        // SPDLOG_INFO("Overloading filtering options with set param
        // values");
        sampler.setMinFilter(textureSampler->getMinFilter());
        sampler.setMagFilter(textureSampler->getMagFilter());
        sampler.setAnisotropy(
            static_cast<float>(textureSampler->getAnisotropy()));

        // Currently leaving this commented out, but this is for 3d
        // textures, which are not currently expected to be loaded
        // as time of writing.
        // sampler.setWrapModeR(textureSampler->getWrapModeR());

        sampler.setWrapModeS(textureSampler->getWrapModeS());
        sampler.setWrapModeT(textureSampler->getWrapModeT());
      }

      if (!foundResource->second.getData().has_value()) {
        spdlog::warn(
            "Got to a case where a texture resource data was not loaded "
            "before trying to "
            "apply to a material.");
        return;
      }

      const auto texture = foundResource->second.getData().value();
      materialInstance->setParameter(szParamName, texture, sampler);
    } break;

    default: {
      SPDLOG_WARN("Type template not setup yet, see {}", __FUNCTION__);
    } break;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void MaterialDefinitions::vSetMaterialInstancePropertiesFromMyPropertyMap(
    const filament::Material* materialResult,
    filament::MaterialInstance* materialInstance,
    const TextureMap& loadedTextures) const {
  const auto count = materialResult->getParameterCount();
  std::vector<filament::Material::ParameterInfo> parameters(count);

  if (const auto actual =
          materialResult->getParameters(parameters.data(), count);
      count != actual || actual != parameters.size()) {
    spdlog::warn(
        "Count of parameters from the material instance and loaded material do "
        "not match; doesn't technically need to, but not ideal and could leave "
        "to undefined results.");
  }

  for (const auto& param : parameters) {
    if (param.name) {
      SPDLOG_TRACE("[Material] name: {}, type: {}", param.name,
                   static_cast<int>(param.type));

      const auto& iter = parameters_.find(param.name);
      if (iter == parameters_.end() || iter->second == nullptr) {
        // This can get pretty spammy, but good if needing to debug further into
        // parameter values.
        SPDLOG_INFO("No default parameter value available for {} {}",
                    __FUNCTION__, param.name);
        continue;
      }
      SPDLOG_TRACE("Setting material param {}", param.name);

      vApplyMaterialParameterToInstance(materialInstance, iter->second.get(),
                                        loadedTextures);
    }
  }
}

}  // namespace plugin_filament_view