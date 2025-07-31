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

#include <core/systems/base/system.h>
#include <memory>

#include <core/components/derived/animation.h>
#include <core/entity/base/entityobject.h>
#include <core/include/literals.h>

namespace flutter {
class PluginRegistrar;
}

namespace plugin_filament_view {

class AnimationSystem : public System {
    friend class Animation;
    friend class EntityObject;

  public:
    AnimationSystem() = default;

    // Disallow copy and assign.
    AnimationSystem(const AnimationSystem&) = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    void onSystemInit() override;
    void update(float deltaTime) override;
    void onDestroy() override;
    void debugPrint() override;

  private:
    void NotifyOfAnimationEvent(
      const EntityGUID entityGuid,
      const AnimationEventType& eType,
      const std::string& eventData
    ) const;
};
}  // namespace plugin_filament_view
