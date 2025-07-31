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

#include "light_system.h"

#include <asio/post.hpp>
#include <core/components/derived/light.h>
#include <core/entity/base/entityobject.h>
#include <core/include/color.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/ecs.h>
#include <core/utils/asserts.h>
#include <core/utils/uuidGenerator.h>
#include <filament/Color.h>
#include <filament/LightManager.h>
#include <filament/Scene.h>
#include <plugins/common/common.h>
#include <utils/EntityManager.h>

namespace plugin_filament_view {
using filament::math::float3;
using filament::math::mat3f;
using filament::math::mat4f;

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::vCreateDefaultLight() {
  SPDLOG_DEBUG("{}", __FUNCTION__);

  m_poDefaultLight = std::make_shared<EntityObject>("DefaultLight", generateUUID());
  const auto oLightComp = std::make_shared<Light>();
  ecs->addComponent(m_poDefaultLight->GetGuid(), oLightComp);

  oLightComp->SetIntensity(200);
  oLightComp->SetDirection({0, -1, 0});
  oLightComp->setPosition({0, 5, 0});
  oLightComp->SetCastLight(true);
  // if you're in an closed space (IE Garage), it will self shadow cast
  oLightComp->SetCastShadows(false);

  vBuildLightAndAddToScene(*oLightComp);

  ecs->addEntity(m_poDefaultLight);
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::vBuildLightAndAddToScene(Light& light) {
  vBuildLight(light);
  vAddLightToScene(light);
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::vBuildLight(Light& light) {
  const auto filamentSystem = ecs->getSystem<FilamentSystem>("vBuildLight");
  const auto engine = filamentSystem->getFilamentEngine();

  if (!light.m_poFilamentEntityLight) {
    light.m_poFilamentEntityLight = engine->getEntityManager().create();
  } else {
    vRemoveLightFromScene(light);
  }

  auto builder = filament::LightManager::Builder(light.GetLightType());

  // As of 11.18.2024 it seems like the color ranges are not the same
  // as their documentation expects 0-1 values, but the actual is 0-255 value
  if (!light.GetColor().empty()) {
    auto colorValue = colorOf(light.GetColor());
    builder.color({colorValue[0], colorValue[1], colorValue[2]});
  } else if (light.GetColorTemperature() > 0) {
    auto cct = filament::Color::cct(light.GetColorTemperature());
    auto red = cct.r;
    auto green = cct.g;
    auto blue = cct.b;
    builder.color({red * 255, green * 255, blue * 255});
  } else {
    builder.color({255, 255, 255});
  }

  // Note while not all of these vars are used in every scenario
  // we're expecting filament to throw away the values that are
  // not needed.
  builder.intensity(light.GetIntensity());
  builder.position(light.GetPosition());
  builder.direction(light.GetDirection());
  builder.castLight(light.GetCastLight());
  builder.castShadows(light.GetCastShadows());
  builder.falloff(light.GetFalloffRadius());

  builder.spotLightCone(light.GetSpotLightConeInner(), light.GetSpotLightConeOuter());

  builder.sunAngularRadius(light.GetSunAngularRadius());
  builder.sunHaloSize(light.GetSunHaloSize());
  builder.sunHaloFalloff(light.GetSunHaloFalloff());

  builder.build(*engine, light.m_poFilamentEntityLight);
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::vRemoveLightFromScene(const Light& light) {
  const auto filamentSystem = ecs->getSystem<FilamentSystem>("lightManager::vRemoveLightFromScene");

  const auto scene = filamentSystem->getFilamentScene();

  scene->remove(light.m_poFilamentEntityLight);
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::vAddLightToScene(const Light& light) {
  const auto filamentSystem = ecs->getSystem<FilamentSystem>("lightManager::vAddLightToScene");

  const auto scene = filamentSystem->getFilamentScene();

  scene->addEntity(light.m_poFilamentEntityLight);
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::onSystemInit() {
  vRegisterMessageHandler(
    ECSMessageType::ChangeSceneLightProperties,
    [this](const ECSMessage& msg) {
      SPDLOG_TRACE("ChangeSceneLightProperties");

      const auto guid = msg.getData<EntityGUID>(ECSMessageType::ChangeSceneLightProperties);

      const auto colorValue = msg.getData<std::string>(
        ECSMessageType::ChangeSceneLightPropertiesColorValue
      );

      const auto intensityValue = msg.getData<float>(
        ECSMessageType::ChangeSceneLightPropertiesIntensity
      );

      // find the entity in our list:
      const auto theLight = ecs->getComponent<Light>(guid);
      runtime_assert(theLight != nullptr, fmt::format("Entity({}): Light not found", guid));

      theLight->SetIntensity(intensityValue);
      theLight->SetColor(colorValue);

      vRemoveLightFromScene(*theLight);
      vBuildLightAndAddToScene(*theLight);

      SPDLOG_TRACE("ChangeSceneLightProperties Complete");
    }
  );

  vRegisterMessageHandler(ECSMessageType::ChangeSceneLightTransform, [this](const ECSMessage& msg) {
    SPDLOG_TRACE("ChangeSceneLightTransform");

    const auto guid = msg.getData<EntityGUID>(ECSMessageType::ChangeSceneLightTransform);

    const auto position = msg.getData<float3>(ECSMessageType::Position);

    const auto rotation = msg.getData<float3>(ECSMessageType::Direction);

    // find the entity in our list:
    const auto theLight = ecs->getComponent<Light>(guid);
    runtime_assert(theLight != nullptr, fmt::format("Entity({}): Light not found", guid));
    theLight->setPosition(position);
    theLight->SetDirection(rotation);

    vRemoveLightFromScene(*theLight);
    vBuildLightAndAddToScene(*theLight);

    SPDLOG_TRACE("ChangeSceneLightTransform Complete");
  });
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::update(float /*deltaTime*/) {}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::onDestroy() {
  if (m_poDefaultLight != nullptr) {
    const auto component = ecs->getComponent<Light>(m_poDefaultLight->GetGuid());
    vRemoveLightFromScene(*component);

    m_poDefaultLight.reset();
  }
}

////////////////////////////////////////////////////////////////////////////////////
void LightSystem::DebugPrint() {
  spdlog::debug("{}", __FUNCTION__);

  // TODO Update print out list of lights
}

}  // namespace plugin_filament_view
