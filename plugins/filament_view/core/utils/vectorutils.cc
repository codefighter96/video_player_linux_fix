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

#include <core/entity/derived/model/model.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/transform_system.h>
#include <core/systems/ecs.h>
#include <core/utils/vectorutils.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

filament::math::mat3f VectorUtils::identity3x3() {
  return {filament::math::float3{1.0f, 0.0f, 0.0f},
          filament::math::float3{0.0f, 1.0f, 0.0f},
          filament::math::float3{0.0f, 0.0f, 1.0f}};
}

filament::math::mat4f VectorUtils::identity4x4() {
  return {filament::math::float4(1.0f, 0.0f, 0.0f, 0.0f),
          filament::math::float4(0.0f, 1.0f, 0.0f, 0.0f),
          filament::math::float4(0.0f, 0.0f, 1.0f, 0.0f),
          filament::math::float4(0.0f, 0.0f, 0.0f, 1.0f)};
}

filament::math::float3 VectorUtils::transformPositionVector(
    const filament::math::float3& position,
    const filament::math::mat4f& transform) {
  // Apply the transformation matrix to the position vector
  return filament::math::float3(
      transform[0].x * position.x + transform[1].x * position.y +
          transform[2].x * position.z + transform[3].x,
      transform[0].y * position.x + transform[1].y * position.y +
          transform[2].y * position.z + transform[3].y,
      transform[0].z * position.x + transform[1].z * position.y +
          transform[2].z * position.z + transform[3].z);
}

filament::math::float3 VectorUtils::transformScaleVector(
  const filament::math::float3& scale,
  const filament::math::mat4f& transform
) {
  // Extract the scaling factors from the transformation matrix
  filament::math::float3 scaleFactors = {
    length(transform[0].xyz),
    length(transform[1].xyz),
    length(transform[2].xyz),
  };

  // Apply the scaling factors to the scale vector
  return filament::math::float3(
      scale.x * scaleFactors.x,
      scale.y * scaleFactors.y,
      scale.z * scaleFactors.z
  );
}

}  // namespace plugin_filament_view
