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

#include <core/systems/base/ecsystem.h>
#include <filament/Box.h>
#include <filament/Engine.h>
#include <math/vec3.h>
#include <utils/EntityManager.h>
#include <list>
#include <memory>
#include <vector>

using ::utils::Entity;

namespace plugin_filament_view {

class DebugLine final {
 public:
  DebugLine(filament::math::float3 startingPoint,
            filament::math::float3 endingPoint,
            filament::Engine* engine,
            std::shared_ptr<Entity> entity,
            float fTimeToLive);
  ~DebugLine() = default;
  void vCleanup(filament::Engine* engine);

  float m_fRemainingTime;
  std::shared_ptr<Entity> m_poEntity;

  filament::VertexBuffer* m_poVertexBuffer = nullptr;
  filament::IndexBuffer* m_poIndexBuffer = nullptr;

  std::vector<::filament::math::float3> vertices_;
  std::vector<unsigned short> indices_;
  filament::Aabb boundingBox_;
};

class DebugLinesSystem final : public ECSystem {
 public:
  DebugLinesSystem() = default;

  void DebugPrint() override;

  // Disallow copy and assign.
  DebugLinesSystem(const DebugLinesSystem&) = delete;
  DebugLinesSystem& operator=(const DebugLinesSystem&) = delete;

  void vUpdate(float fElapsedTime) override;

  void vInitSystem() override;
  void vShutdownSystem() override;

  void vAddLine(::filament::math::float3 startPoint,
                ::filament::math::float3 endPoint,
                float secondsTimeout);

  // called from vShutdownSystem during the systems shutdown routine.
  void vCleanup();

 private:
  bool m_bCurrentlyDrawingDebugLines = false;

  std::list<std::unique_ptr<DebugLine>> ourLines_;
};

}  // namespace plugin_filament_view
