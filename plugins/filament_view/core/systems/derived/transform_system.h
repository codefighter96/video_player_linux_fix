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

#include <core/entity/base/entityobject.h>
#include <core/components/derived/basetransform.h>
#include <core/entity/base/entityobject.h>
#include <core/include/literals.h>
#include <core/systems/base/ecsystem.h>
#include <core/utils/uuidGenerator.h>
#include <utility>

namespace plugin_filament_view {
  
  using ::utils::Entity;

  /**
    * TransformSystem is responsible for updating the transforms of entities in the scene.
    * It handles:
    * - Updating the global transforms after local transforms have been modified
    * - Updating Filament's parent tree based on the ECS hierarchy
    * - TODO: Asynchronous interpolation of transforms 
    */

  class TransformSystem : public ECSystem {
    public:
      TransformSystem() = default;

      void vUpdate(float fElapsedTime) override;
      void vOnInitSystem() override;
      void vShutdownSystem() override;

      [[nodiscard]] size_t GetTypeID() const override {
        return StaticGetTypeID();
      }

      static constexpr size_t StaticGetTypeID() {
        return typeid(TransformSystem).hash_code();
      }
}