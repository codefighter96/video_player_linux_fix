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
#include <core/systems/derived/transform_system.h>
#include <core/utils/deserialize.h>
#include <core/utils/vectorutils.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////
BaseTransform::BaseTransform(const flutter::EncodableMap& params)
    : Component(std::string(__FUNCTION__)),
      local({{kFloat3Zero}, {kFloat3One}, {kQuatfIdentity}}),
      global({kMat4fIdentity}) {

  spdlog::debug("BaseTransform::BaseTransform");
  Deserialize::DecodeParameterWithDefault(kPosition, &(local.position),
                                          params,
                                          kFloat3Zero);
  Deserialize::DecodeParameterWithDefault(kScale, &(local.scale), params,
                                          kFloat3One);
  Deserialize::DecodeParameterWithDefault(kRotation, &(local.rotation), params,
                                          kQuatfIdentity);
  Deserialize::DecodeParameterWithDefaultInt64(kParentId, &_parentId, params,
                                               kNullGuid);

  spdlog::debug("we done");
  DebugPrint("BT -> ");
}

////////////////////////////////////////////////////////////////////////////
void BaseTransform::DebugPrint(const std::string& tabPrefix) const {
  spdlog::debug(tabPrefix + "Local transform:");
  spdlog::debug(tabPrefix + "ParentId: {}", _parentId);
  spdlog::debug(tabPrefix + "Pos: x={}, y={}, z={}",
                local.position.x, local.position.y,
                local.position.z);
  spdlog::debug(tabPrefix + "Scl: x={}, y={}, z={}", local.scale.x, local.scale.y,
                local.scale.z);
  spdlog::debug(tabPrefix + "Rot: w={} x={}, y={}, z={}", local.rotation.w,
                local.rotation.x, local.rotation.y, local.rotation.z);
}

void BaseTransform::SetTransform(const filament::math::mat4f& localMatrix) {
  filament::gltfio::decomposeMatrix(localMatrix, &local.position, &local.rotation, &local.scale);
  _isDirty = true;
}

}  // namespace plugin_filament_view