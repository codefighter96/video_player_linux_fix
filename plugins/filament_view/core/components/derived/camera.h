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

#include <filament/math/mat4.h>
#include <filament/math/quat.h>

#include <core/components/base/component.h>
#include <core/entity/base/entityobject.h>
#include <core/utils/asserts.h>
#include <core/utils/filament_types.h>
#include <core/utils/vectorutils.h>

#include <core/scene/camera/exposure.h>
#include <core/scene/camera/lens_projection.h>
#include <core/scene/camera/projection.h>

namespace plugin_filament_view {

class ViewTarget;

constexpr size_t kDefaultViewId = 0;
constexpr double kDefaultIPD = 0.064;  // Default Inter-Pupillary Distance in meters

class Camera : public Component {
    friend class ViewTarget;

  private:
    /// ID of the ViewTarget associated with this camera
    size_t _viewId = kDefaultViewId;

    /// NOTE: each setting has a 'dirty' flag to
    /// Exposure settings
    std::optional<Exposure> _exposure = std::nullopt;
    bool _dirtyExposure = false;
    /// Projection settings
    /// Projection and Lens are mutually exclusive, when one is set, the other is reset.
    std::optional<Projection> _projection = std::nullopt;
    /// Lens projection settings
    /// Projection and Lens are mutually exclusive, when one is set, the other is reset.
    std::optional<LensProjection> _lens = std::nullopt;
    bool _dirtyProjection = false;

    /// Inter-Pupillary Distance (IPD) in meters.
    /// Used in stereoscopic rendering, where the distance between the eyes is set to this value.
    /// Default value is 0.064 meters (64 mm), which is a common value for human.
    double _ipd = kDefaultIPD;
    bool _dirtyEyes = true;

  public:
    /*
     *  Orbit
     */
    /// Entity that this camera is orbiting around.
    /// If not set, the camera will orbit around its own position.
    EntityGUID orbitOriginEntity = kNullGuid;
    /// Orbit rotation (azimuth and elevation) around the orbit origin.
    filament::math::quatf orbitRotation = VectorUtils::kIdentityQuat;

    /*
     *  Targeting
     */
    /// Controls whether targeting is enabled.
    bool enableTarget = false;
    /// Entity that this camera is looking at.
    EntityGUID targetEntity = kNullGuid;
    /// The target position in world space.
    /// Used only if targetEntity is equal to [kNullGuid].
    filament::math::float3 targetPosition = VectorUtils::kFloat3Zero;

    /*
     *  Dolly
     */
    /// Offset of the camera (head) from its center (rig)
    filament::math::float3 dollyOffset = {0.0f, 0.0f, 0.0f};

  public:
    Camera(
      std::optional<Exposure> exposure,
      std::optional<Projection> projection,
      std::optional<LensProjection> lens,
      size_t viewId = kDefaultViewId
    )
      : Component(std::string(__FUNCTION__)),
        _viewId(viewId),
        _exposure(std::move(exposure)),
        _dirtyExposure(exposure != std::nullopt),
        _projection(std::move(projection)),
        _lens(std::move(lens)),
        _dirtyProjection(projection != std::nullopt || lens != std::nullopt) {
      // check projection and lens are not both set
      runtime_assert(
        !(_projection.has_value() && _lens.has_value()),
        "Projection and LensProjection cannot be set at the same time."
      );
    }

    /// Constructs a Camera component from the given parameters.
    /// TODO(kerberjg): move to deserializer
    explicit Camera(const flutter::EncodableMap& params);

    void DebugPrint(const std::string& tabPrefix) const override;

    /*
     *  Settings
     */
    [[nodiscard]] inline const Exposure* getExposure() const {
      return _exposure.has_value() ? &_exposure.value() : nullptr;
    }

    [[nodiscard]] inline const Projection* getProjection() const {
      return _projection.has_value() ? &_projection.value() : nullptr;
    }

    [[nodiscard]] inline const LensProjection* getLens() const {
      return _lens.has_value() ? &_lens.value() : nullptr;
    }

    [[nodiscard]] inline float getIPD() const { return _ipd; }

    inline void setExposure(const Exposure& exposure) {
      _exposure = exposure;
      _dirtyExposure = true;
    }

    inline void setProjection(const Projection& projection) {
      _projection = projection;
      _lens = std::nullopt;  // Reset lens if projection is set
      _dirtyProjection = true;
    }

    inline void setLens(const LensProjection& lens) {
      _lens = lens;
      _projection = std::nullopt;  // Reset projection if lens is set
      _dirtyProjection = true;
    }

    inline void setIPD(float ipd) {
      _ipd = ipd;
      _dirtyEyes = true;
    }

    [[nodiscard]] inline bool isDirtyExposure() const { return _dirtyExposure; }
    [[nodiscard]] inline bool isDirtyProjection() const { return _dirtyProjection; }
    [[nodiscard]] inline bool isDirtyEyes() const { return _dirtyEyes; }

    [[nodiscard]] inline size_t getViewId() const { return _viewId; }
    inline void setViewId(size_t viewId) { _viewId = viewId; }

    [[nodiscard]] Component* Clone() const override { return new Camera(*this); }

  protected:
    inline void clearDirtyFlags() {
      _dirtyExposure = false;
      _dirtyProjection = false;
      _dirtyEyes = false;
    }

};  // class Camera

}  // namespace plugin_filament_view
