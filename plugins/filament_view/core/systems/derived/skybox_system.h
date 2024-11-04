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

#include <core/include/resource.h>
#include <core/systems/base/ecsystem.h>
#include <future>

namespace plugin_filament_view {

class SkyboxSystem : public ECSystem {
 public:
  SkyboxSystem() = default;

  static std::future<void> Initialize();

  static void setDefaultSkybox();

  static std::future<Resource<std::string_view>> setSkyboxFromHdrAsset(
      const std::string& path,
      bool showSun,
      bool shouldUpdateLight,
      float intensity);

  static std::future<Resource<std::string_view>> setSkyboxFromHdrUrl(
      const std::string& url,
      bool showSun,
      bool shouldUpdateLight,
      float intensity);

  static std::future<Resource<std::string_view>> setSkyboxFromKTXAsset(
      const std::string& path);

  static std::future<Resource<std::string_view>> setSkyboxFromKTXUrl(
      const std::string& url);

  static std::future<Resource<std::string_view>> setSkyboxFromColor(
      const std::string& color);

  static Resource<std::string_view> loadSkyboxFromHdrBuffer(
      const std::vector<uint8_t>& buffer,
      bool showSun,
      bool shouldUpdateLight,
      float intensity);

  static Resource<std::string_view> loadSkyboxFromHdrFile(
      const std::string& assetPath,
      bool showSun,
      bool shouldUpdateLight,
      float intensity);

  // Disallow copy and assign.
  SkyboxSystem(const SkyboxSystem&) = delete;
  SkyboxSystem& operator=(const SkyboxSystem&) = delete;

  [[nodiscard]] size_t GetTypeID() const override { return StaticGetTypeID(); }

  [[nodiscard]] static size_t StaticGetTypeID() {
    return typeid(SkyboxSystem).hash_code();
  }

  void vInitSystem() override;
  void vUpdate(float fElapsedTime) override;
  void vShutdownSystem() override;
  void DebugPrint() override;

 private:
  static void setTransparentSkybox();
};
}  // namespace plugin_filament_view