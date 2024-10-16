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

#include "size.h"

#include <plugins/common/common.h>

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////
Size::Size(const flutter::EncodableMap& params) {
  SPDLOG_TRACE("++Size::Size");
  for (auto& it : params) {
    if (it.second.IsNull())
      continue;

    auto key = std::get<std::string>(it.first);
    if (key == "x" && std::holds_alternative<double>(it.second)) {
      x_ = std::get<double>(it.second);
    } else if (key == "y" && std::holds_alternative<double>(it.second)) {
      y_ = std::get<double>(it.second);
    } else if (key == "z" && std::holds_alternative<double>(it.second)) {
      z_ = std::get<double>(it.second);
    } else if (!it.second.IsNull()) {
      spdlog::debug("[Size] Unhandled Parameter");
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(),
                                                           it.second);
    }
  }
  SPDLOG_TRACE("--Size::Size");
}

////////////////////////////////////////////////////////////////////////////
Size::Size(double x, double y, double z) {
  x_ = x;
  y_ = y;
  z_ = z;
}

////////////////////////////////////////////////////////////////////////////
void Size::DebugPrint(const char* tag) {
  spdlog::debug("++++++++");
  spdlog::debug("{} (Size)", tag);
  if (x_.has_value()) {
    spdlog::debug("\tx: {}", x_.value());
  }
  if (y_.has_value()) {
    spdlog::debug("\ty: {}", y_.value());
  }
  if (z_.has_value()) {
    spdlog::debug("\tz: {}", z_.value());
  }
  spdlog::debug("++++++++");
}

}  // namespace plugin_filament_view
