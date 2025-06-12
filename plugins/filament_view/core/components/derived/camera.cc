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

void Camera::DebugPrint(const std::string& tabPrefix) const {
  spdlog::debug(
    "{}: Camera {{ viewId: {} }",
    tabPrefix,
    _viewId
  );
} // DebugPrint

} // plugin_filament_view
