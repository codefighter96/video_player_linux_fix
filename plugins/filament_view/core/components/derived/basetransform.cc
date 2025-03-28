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
#include "basetransform.h"

#include <core/include/literals.h>
#include <core/utils/deserialize.h>
#include <core/utils/entitytransforms.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////
BaseTransform::BaseTransform(const flutter::EncodableMap& params)
    : Component(std::string(__FUNCTION__)),
      _extentSize(kFloat3Zero),
      local({{kFloat3Zero}, {kFloat3One}, {kQuatfIdentity}}),
      global({kMat4fIdentity}) {
  Deserialize::DecodeParameterWithDefault(kSize, &_extentSize, params,
                                          kFloat3Zero);
  Deserialize::DecodeParameterWithDefault(kPosition, &(local.position),
                                          params,
                                          kFloat3Zero);
  Deserialize::DecodeParameterWithDefault(kScale, &(local.scale), params,
                                          kFloat3One);
  Deserialize::DecodeParameterWithDefault(kRotation, &(local.rotation), params,
                                          kQuatfIdentity);
}

////////////////////////////////////////////////////////////////////////////
void BaseTransform::DebugPrint(const std::string& tabPrefix) const {
  spdlog::debug(tabPrefix + "Local transform:");
  spdlog::debug(tabPrefix + "Pos: x={}, y={}, z={}",
                local.position.x, local.position.y,
                local.position.z);
  spdlog::debug(tabPrefix + "Scl: x={}, y={}, z={}", local.scale.x, local.scale.y,
                local.scale.z);
  spdlog::debug(tabPrefix + "Rot: x={}, y={}, z={} w={}", local.rotation.x,
                local.rotation.y, local.rotation.z, local.rotation.w);
  spdlog::debug(tabPrefix + "Ext: x={}, y={}, z={}", _extentSize.x,
                _extentSize.y, _extentSize.z);
  
  spdlog::debug(tabPrefix + "Global transform:");
  filament::math::float3 tmp;

  tmp = EntityTransforms::oGetTranslationFromTransform(global.matrix);
  spdlog::debug(tabPrefix + "Pos: x={}, y={}, z={}", tmp.x, tmp.y, tmp.z);

  // tmp = EntityTransforms::oGetScaleFromTransform(global);
  // spdlog::debug(tabPrefix + "Scl: x={}, y={}, z={}", tmp.x, tmp.y, tmp.z);
  spdlog::debug(tabPrefix + "Scl: TODO");

  // filament::math::quatf rot = EntityTransforms::oGetRotationFromTransform(global);
  // spdlog::debug(tabPrefix + "Rot: x={}, y={}, z={} w={}", rot.x, rot.y, rot.z, rot.w);
  spdlog::debug(tabPrefix + "Rot: TODO");
}

}  // namespace plugin_filament_view