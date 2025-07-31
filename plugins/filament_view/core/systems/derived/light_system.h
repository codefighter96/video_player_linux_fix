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

#include <core/components/derived/light.h>
#include <core/entity/base/entityobject.h>
#include <future>

#include <core/systems/base/system.h>

namespace plugin_filament_view {

class LightSystem : public System {
    friend class SceneTextDeserializer;
    friend class EntityObject;

  public:
    LightSystem() = default;

    // if after deserialization is complete, and there isn't a light made
    // this will be called to create a simple direct light
    void vCreateDefaultLight();
    void vBuildLight(Light& light);
    void vBuildLightAndAddToScene(Light& light);

    // Disallow copy and assign.
    LightSystem(const LightSystem&) = delete;
    LightSystem& operator=(const LightSystem&) = delete;

    void onSystemInit() override;
    void update(float deltaTime) override;
    void onDestroy() override;
    void DebugPrint() override;

  private:
    // These change the lights in filaments scene
    void vRemoveLightFromScene(const Light& light);
    void vAddLightToScene(const Light& light);

    std::shared_ptr<EntityObject> m_poDefaultLight;
};
}  // namespace plugin_filament_view
