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

#include "shell/platform/common/client_wrapper/include/flutter/encodable_value.h"

#include <filament/Camera.h>

#include <optional>

namespace plugin_filament_view {

class Camera;
class ViewTarget;

class Projection {
    friend class ViewTarget;

  public:
    explicit Projection(const flutter::EncodableMap& params);

    void DebugPrint(const char* tag);

    Projection(const Projection& other)  // NOLINT
      : projection_(other.projection_),
        left_(other.left_),
        right_(other.right_),
        bottom_(other.bottom_),
        top_(other.top_),
        near_(other.near_),
        far_(other.far_),
        fovInDegrees_(other.fovInDegrees_),
        aspect_(other.aspect_),
        fovDirection_(other.fovDirection_) {}

    // Implement the assignment operator for deep copy.
    Projection& operator=(const Projection& other) {
      if (this == &other) {
        return *this;  // Handle self-assignment.
      }
      projection_ = other.projection_;
      left_ = other.left_;
      right_ = other.right_;
      bottom_ = other.bottom_;
      top_ = other.top_;
      near_ = other.near_;
      far_ = other.far_;
      fovInDegrees_ = other.fovInDegrees_;
      aspect_ = other.aspect_;
      fovDirection_ = other.fovDirection_;
      return *this;
    }

    // Add a clone method for creating a deep copy.
    [[nodiscard]] std::unique_ptr<Projection> clone() const {
      return std::make_unique<Projection>(*this);  // Calls the copy constructor
    }

    static const char* getTextForType(::filament::Camera::Projection type);

    static ::filament::Camera::Projection getTypeForText(const std::string& type);

    static const char* getTextForFov(::filament::Camera::Fov fov);

    static ::filament::Camera::Fov getFovForText(const std::string& fov);

  private:
    static constexpr char kTypePerspective[] = "PERSPECTIVE";
    static constexpr char kTypeOrtho[] = "ORTHO";
    static constexpr char kFovVertical[] = "VERTICAL";
    static constexpr char kFovHorizontal[] = "HORIZONTAL";

    /// Denotes the projection type used by this camera.
    std::optional<::filament::Camera::Projection> projection_;

    /// distance in world units from the camera to the left plane, at the near
    /// plane. Precondition: left != right
    std::optional<double> left_;

    /// distance in world units from the camera to the right plane, at the near
    /// plane. Precondition: left != right
    std::optional<double> right_;

    /// distance in world units from the camera to the bottom plane, at the near
    /// plane. Precondition: bottom != top
    std::optional<double> bottom_;

    /// distance in world units from the camera to the top plane, at the near
    /// plane. Precondition: bottom != top
    std::optional<double> top_;

    /// distance in world units from the camera to the near plane.
    ///  The near plane's position in view space is z = -near.
    ///  Precondition: near > 0 for ProjectionType.PERSPECTIVE or near != far for
    ///  ProjectionType.ORTHO.
    std::optional<double> near_;

    /// distance in world units from the camera to the far plane.
    ///  The far plane's position in view space is z = -far.
    ///  Precondition: far > near for ProjectionType.PERSPECTIVE or far != near
    ///  for ProjectionType.ORTHO.
    std::optional<double> far_;

    /// full field-of-view in degrees. 0 < fovInDegrees < 180
    std::optional<double> fovInDegrees_;

    /// aspect ratio width/height. aspect > 0
    std::optional<double> aspect_;

    /// direction of the field-of-view parameter.
    std::optional<::filament::Camera::Fov> fovDirection_;
};
}  // namespace plugin_filament_view
