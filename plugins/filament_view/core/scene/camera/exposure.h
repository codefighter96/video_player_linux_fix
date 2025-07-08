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

#include "shell/platform/common/client_wrapper/include/flutter/encodable_value.h"

#include <memory>
#include <optional>

class ViewTarget;

namespace plugin_filament_view {

class Exposure {
    friend class ViewTarget;

  public:
    explicit Exposure(const flutter::EncodableMap& params);

    void DebugPrint(const char* tag);

    Exposure(const Exposure& other) = default;

    Exposure& operator=(const Exposure& other) {
      if (this == &other) {
        return *this;
      }
      aperture_ = other.aperture_;
      shutterSpeed_ = other.shutterSpeed_;
      sensitivity_ = other.sensitivity_;
      exposure_ = other.exposure_;
      return *this;
    }

    [[nodiscard]] std::unique_ptr<Exposure> clone() const {
      return std::make_unique<Exposure>(*this);  // Calls the copy constructor
    }

  private:
    /// Aperture in f-stops, clamped between 0.5 and 64. A lower aperture value
    /// increases the exposure, leading to a brighter scene. Realistic values are
    /// between 0.95 and 32.
    std::optional<float> aperture_;

    /// shutterSpeed – Shutter speed in seconds,
    /// clamped between 1/25,000 and 60. A lower shutter speed increases the
    /// exposure. Realistic values are between 1/8000 and 30. sensitivity
    std::optional<float> shutterSpeed_;

    /// Sensitivity in ISO, clamped between 10 and 204,800.
    /// A higher sensitivity increases the exposure. Realistic values are between
    /// 50 and 25600.
    std::optional<float> sensitivity_;

    /// Sets this camera's exposure directly.
    std::optional<float> exposure_;
};
}  // namespace plugin_filament_view
