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

#include "indirect_light.h"

#include <plugins/common/common.h>

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////
IndirectLight::IndirectLight(std::string assetPath, std::string url, const float intensity)
  : assetPath_(std::move(assetPath)),
    url_(std::move(url)),
    intensity_(intensity) {}

////////////////////////////////////////////////////////////////////////////
IndirectLight::~IndirectLight() = default;

////////////////////////////////////////////////////////////////////////////
std::unique_ptr<IndirectLight> IndirectLight::Deserialize(const flutter::EncodableMap& params) {
  SPDLOG_TRACE("++IndirectLight::Deserialize");

  std::optional<int32_t> type;
  std::optional<std::string> assetPath;
  std::optional<std::string> url;
  std::optional<double> intensity;

  for (const auto& [fst, snd] : params) {
    if (snd.IsNull()) continue;

    if (auto key = std::get<std::string>(fst);
        key == "assetPath" && std::holds_alternative<std::string>(snd)) {
      assetPath = std::get<std::string>(snd);
    } else if (key == "url" && std::holds_alternative<std::string>(snd)) {
      url = std::get<std::string>(snd);
    } else if (key == "intensity" && std::holds_alternative<double>(snd)) {
      intensity = std::get<double>(snd);
    } else if (key == "lightType" && std::holds_alternative<int32_t>(snd)) {
      type = std::get<int32_t>(snd);
    } /*else if (!it.second.IsNull()) {
      spdlog::debug("[IndirectLight] Unhandled Parameter");
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(),
                                                           it.second);
    }*/
  }

  if (type.has_value()) {
    switch (type.value()) {
      case 1:
        spdlog::debug("[IndirectLight] Type: KtxIndirectLight");
        return std::make_unique<KtxIndirectLight>(std::move(assetPath), std::move(url), intensity);

      case 2:
        spdlog::debug("[IndirectLight] Type: HdrIndirectLight");
        return std::make_unique<HdrIndirectLight>(std::move(assetPath), std::move(url), intensity);

      case 3:
        spdlog::debug("[IndirectLight] Type: DefaultIndirectLight");
        return std::make_unique<DefaultIndirectLight>();

      default:
        spdlog::error("[IndirectLight] Type: Unknown DefaultIndirectLight");
        break;
    }
  } else {
    spdlog::error("[IndirectLight] No Type Value");
  }

  SPDLOG_TRACE("--IndirectLight::Deserialize");
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////
void IndirectLight::DebugPrint(const char* tag) {
  spdlog::debug("++++++++");
  spdlog::debug("{} (Light)", tag);
  spdlog::debug("\tintensity: {}", intensity_);
}

////////////////////////////////////////////////////////////////////////////
void DefaultIndirectLight::DebugPrint(const char* tag) {
  spdlog::debug("++++++++");
  spdlog::debug("{} (DefaultIndirectLight)", tag);
  spdlog::debug("\tintensity: {}", intensity_);
}

}  // namespace plugin_filament_view
