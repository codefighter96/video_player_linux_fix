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

#include <filament/gltfio/math.h>
#include <filament/gltfio/FilamentAsset.h>
#include <filament/math/TMatHelpers.h>
#include <filament/math/mat4.h>
#include <filament/math/quat.h>
#include <filament/math/vec3.h>
#include <filament/TransformManager.h>
#include <memory>

#include <core/utils/filament_types.h>

namespace plugin_filament_view {

class VectorUtils {
 public:
  // Utility functions to create identity matrices
  static filament::math::mat3f identity3x3();
  static filament::math::mat4f identity4x4();

  static filament::math::float3 transformPositionVector(
    const filament::math::float3& pos,
    const filament::math::mat4f& transform);
  static filament::math::float3 transformScaleVector(
    const filament::math::float3& scale,
    const filament::math::mat4f& transform);
};

}  // namespace plugin_filament_view
