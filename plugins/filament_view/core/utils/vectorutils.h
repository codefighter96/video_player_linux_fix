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
#include <filament/math/mat3.h>
#include <filament/math/mat4.h>
#include <filament/math/quat.h>
#include <filament/math/scalar.h>
#include <filament/math/vec2.h>
#include <filament/math/vec3.h>
#include <filament/math/vec4.h>

namespace plugin_filament_view {

class VectorUtils {
  public:
    /// Static identity quaternion
    // NOTE: Filament's quaternion constructor takes WXYZ, not XYZW!!!
    static constexpr filament::math::quatf kIdentityQuat =
      filament::math::quatf(1.0f, 0.0f, 0.0f, 0.0f);

    /// Static identity 3x3 matrix
    static constexpr filament::math::mat3f kIdentity3x3 = filament::math::mat3f(
      1.0f,
      0.0f,
      0.0f,  //
      0.0f,
      1.0f,
      0.0f,  //
      0.0f,
      0.0f,
      1.0f  //
    );

    /// Static identity 4x4 matrix
    static constexpr filament::math::mat4f kIdentity4x4 = filament::math::mat4f(
      1.0f,
      0.0f,
      0.0f,
      0.0f,  //
      0.0f,
      1.0f,
      0.0f,
      0.0f,  //
      0.0f,
      0.0f,
      1.0f,
      0.0f,  //
      0.0f,
      0.0f,
      0.0f,
      1.0f  //
    );

    // Utility functions to create identity matrices
    static filament::math::mat3f identity3x3();
    static filament::math::mat4f identity4x4();

    static filament::math::float3 transformPositionVector(
      const filament::math::float3& pos,
      const filament::math::mat4f& transform
    );
    static filament::math::float3 transformScaleVector(
      const filament::math::float3& scale,
      const filament::math::mat4f& transform
    );
};

}  // namespace plugin_filament_view
