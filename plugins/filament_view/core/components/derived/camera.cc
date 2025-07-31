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
#include "camera.h"

#include <core/include/literals.h>
#include <core/systems/derived/transform_system.h>
#include <core/utils/deserialize.h>
#include <core/utils/vectorutils.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

Camera::Camera(const flutter::EncodableMap& params)
  : Component(std::string(__FUNCTION__)) {
  // Camera (head)
  auto exposureParams = Deserialize::DecodeOptionalParameter<flutter::EncodableMap>(
    kExposure, params
  );
  if (exposureParams.has_value()) {
    _exposure = Exposure(exposureParams.value());
    _dirtyExposure = true;
  }

  auto projectionParams = Deserialize::DecodeOptionalParameter<flutter::EncodableMap>(
    kProjection, params
  );
  if (projectionParams.has_value()) {
    _projection = Projection(projectionParams.value());
    _dirtyProjection = true;
  }

  auto lensParams = Deserialize::DecodeOptionalParameter<flutter::EncodableMap>(
    kLensProjection, params
  );
  if (lensParams.has_value()) {
    _lens = LensProjection(lensParams.value());
    _dirtyProjection = true;

    // Reset projection if lens is set
    if (_projection.has_value()) {
      spdlog::warn(
        "LensProjection is set, resetting Projection. LensProjection will be used instead."
      );
      _projection.reset();
    }
  }

  // _ipd = 0; // leave default in header, currently not supported in Dart
  _dirtyEyes = false;

  int64_t tmpViewId = 0;
  Deserialize::DecodeParameterWithDefaultInt64(kViewId, &tmpViewId, params, kDefaultViewId);
  _viewId = static_cast<size_t>(tmpViewId);

  Deserialize::DecodeParameterWithDefault(kDollyOffset, &dollyOffset, params, kFloat3Zero);

  Deserialize::DecodeParameterWithDefaultInt64(
    kOrbitOriginEntity, &orbitOriginEntity, params, kNullGuid
  );

  Deserialize::DecodeParameterWithDefault(
    kOrbitRotation, &orbitRotation, params, VectorUtils::kIdentityQuat
  );

  Deserialize::DecodeParameterWithDefaultInt64(kTargetEntity, &targetEntity, params, kNullGuid);

  const bool hasTargetPos = Deserialize::DecodeParameterWithDefault(
    kTargetPoint, &targetPoint, params, VectorUtils::kFloat3Zero
  );

  // Check if target position is set, if so, enable targeting
  enableTarget = hasTargetPos || targetEntity != kNullGuid;

}  // Camera

void Camera::debugPrint(const std::string& tabPrefix) const {
  spdlog::debug("{}: Camera {{ viewId: {} }", tabPrefix, _viewId);
}  // debugPrint

}  // namespace plugin_filament_view
