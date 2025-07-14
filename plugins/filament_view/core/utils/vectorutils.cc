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

filament::math::quatf VectorUtils::fromEulerAngles(float yaw, float pitch, float roll) {
  const float halfYaw = yaw * 0.5f;
  const float halfPitch = pitch * 0.5f;
  const float halfRoll = roll * 0.5f;
  const float cosYaw = cos(halfYaw);
  const float sinYaw = sin(halfYaw);
  const float cosPitch = cos(halfPitch);
  const float sinPitch = sin(halfPitch);
  const float cosRoll = cos(halfRoll);
  const float sinRoll = sin(halfRoll);

  const float x = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
  const float y = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
  const float z = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
  const float w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

  return {w, x, y, z};
}

filament::math::quatf VectorUtils::lookAt(
  const filament::math::float3& position,
  const filament::math::float3& target
) {
  const filament::math::float3 delta = target - position;

  // Calculate the XZ plane direction
  filament::math::float2 xzDir = filament::math::float2(delta.z, delta.x);
  // Calculate the lY plane direction
  filament::math::float2 lYDir = filament::math::float2(length(xzDir), delta.y);

  // normalize vectors first
  xzDir = normalize(xzDir);
  lYDir = normalize(lYDir);

  // Calculate the rotation angle around the Y axis
  float azimuth = atan2(xzDir.y, xzDir.x);
  // Calculate the rotation angle around the X axis
  float elevation = atan2(lYDir.y, lYDir.x);

  // Create the quaternion representing the rotation
  return VectorUtils::fromEulerAngles(
    azimuth + M_PI,  // Yaw (Y-axis rotation)
    elevation,       // Pitch (X-axis rotation)
    0.0f             // Roll (Z-axis rotation, not used here)
  );
}

}  // namespace plugin_filament_view
