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

#include "vectorutils.h"

#include <plugins/common/common.h>

namespace plugin_filament_view {

filament::math::mat3f VectorUtils::identity3x3() {
  return {
    filament::math::float3{1.0f, 0.0f, 0.0f}, filament::math::float3{0.0f, 1.0f, 0.0f},
    filament::math::float3{0.0f, 0.0f, 1.0f}
  };
}

filament::math::mat4f VectorUtils::identity4x4() {
  return {
    filament::math::float4(1.0f, 0.0f, 0.0f, 0.0f), filament::math::float4(0.0f, 1.0f, 0.0f, 0.0f),
    filament::math::float4(0.0f, 0.0f, 1.0f, 0.0f), filament::math::float4(0.0f, 0.0f, 0.0f, 1.0f)
  };
}

filament::math::float3 VectorUtils::transformPositionVector(
  const filament::math::float3& pos,
  const filament::math::mat4f& transform
) {
  // Apply the transformation matrix to the position vector
  return {
    transform[0].x * pos.x + transform[1].x * pos.y + transform[2].x * pos.z + transform[3].x,
    transform[0].y * pos.x + transform[1].y * pos.y + transform[2].y * pos.z + transform[3].y,
    transform[0].z * pos.x + transform[1].z * pos.y + transform[2].z * pos.z + transform[3].z
  };
}

filament::math::float3 VectorUtils::transformScaleVector(
  const filament::math::float3& scale,
  const filament::math::mat4f& transform
) {
  // Extract the scaling factors from the transformation matrix
  // Apply the scaling factors to the scale vector
  return {
    scale.x * length(transform[0].xyz), scale.y * length(transform[1].xyz),
    scale.z * length(transform[2].xyz)
  };
}

filament::math::quatf VectorUtils::lookAt(
  const filament::math::float3& position,
  const filament::math::float3& target,
  const filament::math::float3& up
) {
  const filament::math::float3 forward = normalize(target - position);
  const float dotp = dot(forward, up);

  constexpr float epsilon = 0.000001f;
  // If the forward vector is almost opposite to the up vector
  // we need to rotate 180 degrees around the up vector to avoid gimbal lock.
  if (abs(dotp + 1.0f) < epsilon) {
    /// TODO: might have to set global rotation instead of local
    return {up.x, up.y, up.z, M_PI};
  }
  // If the forward vector is almost aligned with the up vector,
  // we canset the rotation to identity.
  if (abs(dotp - 1.0f) < epsilon) {
    return kIdentityQuat;
  }

  // Regular case
  const float rotAngle = std::acos(dotp);
  const auto rotAxis = normalize(cross(VectorUtils::kForward3, forward));

  return filament::math::quatf::fromAxisAngle(rotAxis, rotAngle);
}

}  // namespace plugin_filament_view
