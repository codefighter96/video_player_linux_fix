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

#include <list>
#include <memory>
#include <vector>

#include <filament/Box.h>
#include <filament/Engine.h>
#include <filament/utils/EntityManager.h>
#include <math/vec3.h>

#include <core/systems/base/system.h>

namespace plugin_filament_view {

class DebugLine final {
  public:
    DebugLine(
      filament::math::float3 startingPoint,
      filament::math::float3 endingPoint,
      filament::Engine* engine,
      FilamentEntity entity,
      float fTimeToLive
    );
    ~DebugLine() = default;
    void Cleanup(filament::Engine* engine);

    float m_fRemainingTime;
    FilamentEntity _fEntity;

    filament::VertexBuffer* m_poVertexBuffer = nullptr;
    filament::IndexBuffer* m_poIndexBuffer = nullptr;

    std::vector<::filament::math::float3> vertices_;
    std::vector<unsigned short> indices_;
    filament::Aabb boundingBox_;
};

class DebugLinesSystem final : public System {
  public:
    DebugLinesSystem() = default;

    void debugPrint() override;

    // Disallow copy and assign.
    DebugLinesSystem(const DebugLinesSystem&) = delete;
    DebugLinesSystem& operator=(const DebugLinesSystem&) = delete;

    void update(double deltaTime) override;

    void onSystemInit() override;
    void onDestroy() override;

    void AddLine(
      ::filament::math::float3 startPoint,
      ::filament::math::float3 endPoint,
      float secondsTimeout
    );

    // called from onDestroy during the systems shutdown routine.
    void Cleanup();

  private:
    bool m_bCurrentlyDrawingDebugLines = false;

    std::list<std::unique_ptr<DebugLine>> ourLines_;
};

}  // namespace plugin_filament_view
