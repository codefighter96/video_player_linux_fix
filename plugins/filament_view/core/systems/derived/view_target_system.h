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

#include <core/scene/view_target.h>
#include <core/systems/base/system.h>
#include <filament/Engine.h>
#include <flutter_desktop_engine_state.h>

namespace plugin_filament_view {

class ViewTarget;
class Camera;

constexpr size_t kMainViewId = 0;  // The main view target is always the first one in the list.

class ViewTargetSystem : public System {
  public:
    ViewTargetSystem() = default;

    // Disallow copy and assign.
    ViewTargetSystem(const ViewTargetSystem&) = delete;
    ViewTargetSystem& operator=(const ViewTargetSystem&) = delete;

    void onSystemInit() override;
    void update(float deltaTime) override;
    void onDestroy() override;
    void debugPrint() override;

    // Returns the current iter that you put into the list.
    size_t nSetupViewTargetFromDesktopState(
      int32_t top,
      int32_t left,
      FlutterDesktopEngineState* state
    );

    void KickOffFrameRenderingLoops() const;

    /// Returns the view target at the specified index.
    [[nodiscard]] ViewTarget* getViewTarget(size_t index) const;

    /// Returns the main view target, which is the first one in the list.
    [[nodiscard]] inline ViewTarget* getMainViewTarget() const {
      return getViewTarget(kMainViewId);
    }

    /// Sets a camera as the main camera for the given view target.
    void setViewCamera(size_t viewId, EntityGUID cameraId);

    /// Initializes a camera entity
    void initializeEntity(EntityGUID entityGuid);

  private:
    std::vector<std::unique_ptr<ViewTarget>> _viewTargets;
};
}  // namespace plugin_filament_view
