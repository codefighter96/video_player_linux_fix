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

#include <core/scene/camera/exposure.h>
#include <core/scene/camera/lens_projection.h>
#include <core/scene/camera/projection.h>

namespace plugin_filament_view {

class ViewTarget;

class Camera : public Component {
  friend class ViewTarget;
  
  private:
    /// ID of the ViewTarget associated with this camera
    size_t _viewId = 0;

    /// NOTE: each setting has a 'dirty' flag to 
    // Exposure settings
    Exposure _exposure;
    bool _dirtyExposure;
    // Projection settings
    Projection _projection;
    bool _dirtyProjection;
    // Lens projection settings
    LensProjection _lens;
    bool _dirtyLens;

    /// Inter-Pupillary Distance (IPD) in meters.
    /// Used in stereoscopic rendering, where the distance between the eyes is set to this value.
    /// Default value is 0.064 meters (64 mm), which is a common value for human.
    float _ipd;
    bool _dirtyEyes = true;

  public:
    Camera(
      Exposure exposure,
      Projection projection,
      LensProjection lens,
      size_t viewId = 0
    ) : Component(std::string(__FUNCTION__)),
        _viewId(viewId),
        _exposure(std::move(exposure)),
        _dirtyExposure(true),
        _projection(std::move(projection)),
        _dirtyProjection(true),
        _lens(std::move(lens)),
        _dirtyLens(true),
        _ipd(0),
        _dirtyEyes(false) {}
    // explicit Camera(const flutter::EncodableMap& params);

    void DebugPrint(const std::string& tabPrefix) const override;

    /*
     *  Settings
     */
    [[nodiscard]] inline const Exposure& getExposure() const { return _exposure; }
    [[nodiscard]] inline const Projection& getProjection() const { return _projection; }
    [[nodiscard]] inline const LensProjection& getLens() const { return _lens; }
    [[nodiscard]] inline float getIPD() const { return _ipd; }

    inline void setExposure(const Exposure& exposure) {
      _exposure = exposure;
      _dirtyExposure = true;
    }

    inline void setProjection(const Projection& projection) {
      _projection = projection;
      _dirtyProjection = true;
    }

    inline void setLens(const LensProjection& lens) {
      _lens = lens;
      _dirtyLens = true;
    }

    inline void setIPD(float ipd) {
      _ipd = ipd;
      _dirtyEyes = true;
    }

    [[nodiscard]] inline bool isDirtyExposure() const { return _dirtyExposure; }
    [[nodiscard]] inline bool isDirtyProjection() const { return _dirtyProjection; }
    [[nodiscard]] inline bool isDirtyLens() const { return _dirtyLens; }
    [[nodiscard]] inline bool isDirtyEyes() const { return _dirtyEyes; }

    [[nodiscard]] inline size_t getViewId() const { return _viewId; }
    inline void setViewId(size_t viewId) {
      _viewId = viewId;
    }

    [[nodiscard]] Component* Clone() const override {
      return new Camera(*this);
    }
  
  protected:
    inline void clearDirtyFlags() {
      _dirtyExposure = false;
      _dirtyProjection = false;
      _dirtyLens = false;
      _dirtyEyes = false;
    }

};  // class Camera

}  // namespace plugin_filament_view
