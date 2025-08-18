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

#include "indirect_light_system.h"

#include <asio/post.hpp>
#include <core/include/literals.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/ecs.h>
#include <core/utils/hdr_loader.h>
#include <filament/Texture.h>
#include <filesystem>
#include <memory>
#include <plugins/common/common.h>
#include <utility>

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////////////
void IndirectLightSystem::setDefaultIndirectLight() {
  SPDLOG_TRACE("++IndirectLightSystem::setDefaultIndirectLight");
  indirect_light_ = std::make_unique<DefaultIndirectLight>();
  setIndirectLight(indirect_light_.get());
}

////////////////////////////////////////////////////////////////////////////////////
IndirectLightSystem::~IndirectLightSystem() = default;

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> IndirectLightSystem::setIndirectLight(
  DefaultIndirectLight* indirectLight
) {
  const auto promise(std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());

  // Note: LightState to custom model viewer was done here.
  // todo copy values to internal var.

  if (!indirectLight) {
    promise->set_value(Resource<std::string_view>::Error("Light is null"));
    return future;
  }

  const asio::io_context::strand& strand_(*ecs->getStrand());

  post(strand_, [&, promise, indirectLight] {
    auto builder = filament::IndirectLight::Builder();
    builder.intensity(indirectLight->getIntensity());
    builder.radiance(
      static_cast<uint8_t>(indirectLight->radiance_.size()), indirectLight->radiance_.data()
    );
    builder.irradiance(
      static_cast<uint8_t>(indirectLight->irradiance_.size()), indirectLight->irradiance_.data()
    );
    if (indirectLight->rotation_.has_value()) {
      builder.rotation(indirectLight->rotation_.value());
    }

    const auto filamentSystem = ecs->getSystem<FilamentSystem>("setIndirectLight");
    const auto engine = filamentSystem->getFilamentEngine();

    builder.build(*engine);

    promise->set_value(Resource<std::string_view>::Success("changed Light successfully"));
  });
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> IndirectLightSystem::
  setIndirectLightFromKtxAsset(const std::string& /*path*/, double /*intensity*/) {
  const auto promise(std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());

  const asio::io_context::strand& strand_(*ecs->getStrand());

  post(strand_, [&, promise /*, intensity*/] {
    promise->set_value(Resource<std::string_view>::Error("Not implemented"));
  });
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> IndirectLightSystem::
  setIndirectLightFromKtxUrl(const std::string& /*url*/, double /*intensity*/) {
  const auto promise(std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());

  const asio::io_context::strand& strand_(*ecs->getStrand());

  post(strand_, [&, promise /*, intensity*/] {
    promise->set_value(Resource<std::string_view>::Error("Not implemented"));
  });
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
Resource<std::string_view> IndirectLightSystem::loadIndirectLightHdrFromFile(
  const std::string& asset_path,
  const double intensity
) {
  const auto filamentSystem = ecs->getSystem<FilamentSystem>("loadIndirectLightHdrFromFile");
  const auto engine = filamentSystem->getFilamentEngine();

  filament::Texture* texture;
  try {
    texture = HDRLoader::createTexture(engine, asset_path);
  } catch (...) {
    return Resource<std::string_view>::Error("Could not decode HDR file");
  }
  const auto skyboxTexture = filamentSystem->getIBLProfiler()->createCubeMapTexture(texture);
  engine->destroy(texture);

  const auto reflections = filamentSystem->getIBLProfiler()->getLightReflection(skyboxTexture);

  const auto ibl = filament::IndirectLight::Builder()
                     .reflections(reflections)
                     .intensity(static_cast<float>(intensity))
                     .build(*engine);

  const auto prevIndirectLight = filamentSystem->getFilamentScene()->getIndirectLight();
  if (prevIndirectLight) {
    engine->destroy(prevIndirectLight);
  }

  filamentSystem->getFilamentScene()->setIndirectLight(ibl);

  return Resource<std::string_view>::Success("loaded Indirect light successfully");
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> IndirectLightSystem::setIndirectLightFromHdrAsset(
  const std::string& path,
  double intensity
) {
  const auto promise(std::make_shared<std::promise<Resource<std::string_view>>>());
  auto future(promise->get_future());

  const asio::io_context::strand& strand_(*ecs->getStrand());
  const auto assetPath = ecs->getConfigValue<std::string>(kAssetPath);

  post(strand_, [&, promise, path = path, intensity, assetPath] {
    std::filesystem::path asset_path(assetPath);
    asset_path /= path;

    if (path.empty() || !exists(asset_path)) {
      promise->set_value(Resource<std::string_view>::Error("Asset path not valid"));
    }
    try {
      promise->set_value(loadIndirectLightHdrFromFile(asset_path.c_str(), intensity));
    } catch (...) {
      promise->set_value(Resource<std::string_view>::Error("Couldn't changed Light from asset"));
    }
  });
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
std::future<Resource<std::string_view>> IndirectLightSystem::
  setIndirectLightFromHdrUrl(const std::string& /*url*/, double /*intensity*/) {
  const auto promise(std::make_shared<std::promise<Resource<std::string_view>>>());

  const asio::io_context::strand& strand_(*ecs->getStrand());

  auto future(promise->get_future());
  post(strand_, [&, promise /*, intensity*/] {
    promise->set_value(Resource<std::string_view>::Error("Not implemented"));
  });
  return future;
}

////////////////////////////////////////////////////////////////////////////////////
void IndirectLightSystem::onSystemInit() {
  setDefaultIndirectLight();

  registerMessageHandler(
    ECSMessageType::ChangeSceneIndirectLightProperties,
    [this](const ECSMessage& msg) {
      spdlog::debug("ChangeSceneIndirectLightProperties");

      const auto intensityValue = msg.getData<float>(
        ECSMessageType::ChangeSceneIndirectLightPropertiesIntensity
      );
      indirect_light_->setIntensity(intensityValue);
      setIndirectLight(indirect_light_.get());

      spdlog::debug("ChangeSceneIndirectLightProperties Complete");
    }
  );
}

////////////////////////////////////////////////////////////////////////////////////
void IndirectLightSystem::update(double /*deltaTime*/) {}

////////////////////////////////////////////////////////////////////////////////////
void IndirectLightSystem::onDestroy() {
  const auto filamentSystem = ecs->getSystem<FilamentSystem>("setIndirectLight");
  const auto engine = filamentSystem->getFilamentEngine();

  const auto prevIndirectLight = filamentSystem->getFilamentScene()->getIndirectLight();
  if (prevIndirectLight) {
    engine->destroy(prevIndirectLight);
  }

  indirect_light_.reset();
}

////////////////////////////////////////////////////////////////////////////////////
void IndirectLightSystem::debugPrint() { spdlog::debug("{}", __FUNCTION__); }

}  // namespace plugin_filament_view
