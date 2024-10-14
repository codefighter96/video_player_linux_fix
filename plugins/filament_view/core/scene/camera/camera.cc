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

#include <memory>

#include "core/include/literals.h"
#include "core/utils/deserialize.h"
#include "plugins/common/common.h"

namespace plugin_filament_view {
Camera::Camera(const flutter::EncodableMap& params)
    : inertia_rotationSpeed_(0.05f),
      inertia_velocityFactor_(0.2f),
      inertia_decayFactor_(0.86f),
      pan_angleCapX_(15),
      pan_angleCapY_(20),
      zoom_minCap_(3),
      zoom_maxCap_(10),
      current_zoom_radius_(5),
      current_pitch_addition_(0),
      current_yaw_addition_(0) {
  SPDLOG_TRACE("++Camera::Camera");

  // Currently variables not coming over from dart, Backlogged.
  eCustomCameraMode_ = CustomCameraMode::Unset;
  fCurrentOrbitAngle_ = 0;
  orbitHomePosition_ = std::make_unique<::filament::math::float3>(0, 3, 0);
  forceSingleFrameUpdate_ = false;

  Deserialize::DecodeParameterWithDefault(
      kCamera_Inertia_RotationSpeed, &inertia_rotationSpeed_, params, 0.05f);

  Deserialize::DecodeParameterWithDefault(
      kCamera_Inertia_VelocityFactor, &inertia_velocityFactor_, params, 0.2f);

  Deserialize::DecodeParameterWithDefault(kCamera_Inertia_DecayFactor,
                                          &inertia_decayFactor_, params, 0.86f);

  Deserialize::DecodeParameterWithDefault(kCamera_Pan_angleCapX,
                                          &pan_angleCapX_, params, 15);

  Deserialize::DecodeParameterWithDefault(kCamera_Pan_angleCapY,
                                          &pan_angleCapY_, params, 20);

  Deserialize::DecodeParameterWithDefault(kCamera_Zoom_minCap, &zoom_minCap_,
                                          params, 3.0f);

  Deserialize::DecodeParameterWithDefault(kCamera_Zoom_maxCap, &zoom_maxCap_,
                                          params, 10.0f);

  for (const auto& it : params) {
    auto key = std::get<std::string>(it.first);

    if (key == "exposure") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        exposure_ = std::make_unique<Exposure>(
            std::get<flutter::EncodableMap>(it.second));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        exposure_ = std::make_unique<Exposure>(flutter::EncodableMap{});
      }
    } else if (key == "projection") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        projection_ = std::make_unique<Projection>(
            std::get<flutter::EncodableMap>(it.second));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flutter::EncodableMap map = {
            {flutter::EncodableValue("focalLength"), flutter::EncodableValue()},
            {flutter::EncodableValue("aspect"), flutter::EncodableValue()},
            {flutter::EncodableValue("near"), flutter::EncodableValue()},
            {flutter::EncodableValue("far"), flutter::EncodableValue()}};
        projection_ = std::make_unique<Projection>(map);
      }
    } else if (key == "lensProjection") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        lensProjection_ = std::make_unique<LensProjection>(
            std::get<flutter::EncodableMap>(it.second));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flutter::EncodableMap map = {
            {flutter::EncodableValue("focalLength"), flutter::EncodableValue()},
            {flutter::EncodableValue("aspect"), flutter::EncodableValue()},
            {flutter::EncodableValue("near"), flutter::EncodableValue()},
            {flutter::EncodableValue("far"), flutter::EncodableValue()}};
        lensProjection_ = std::make_unique<LensProjection>(map);
      }
    } else if (key == "flightMaxMoveSpeed") {
      if (std::holds_alternative<double>(it.second)) {
        flightMaxMoveSpeed_ = std::get<double>(it.second);
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flightMaxMoveSpeed_ = 10;
      }
    } else if (key == "flightMoveDamping") {
      if (std::holds_alternative<double>(it.second)) {
        flightMoveDamping_ = std::get<double>(it.second);
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flightMoveDamping_ = 15.0;
      }
    } else if (key == "flightSpeedSteps") {
      if (std::holds_alternative<int64_t>(it.second)) {
        flightSpeedSteps_ = std::get<int64_t>(it.second);
      } else {
        flightSpeedSteps_ = 80;
      }
    } else if (key == "flightStartOrientation") {
      if (std::holds_alternative<flutter::EncodableList>(it.second)) {
        auto list = std::get<flutter::EncodableList>(it.second);
        flightStartOrientation_ = std::make_unique<std::vector<float>>();
        for (const auto& item : list) {
          flightStartOrientation_->emplace_back(std::get<double>(item));
        }
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flightStartOrientation_ = std::make_unique<std::vector<float>>();
        flightStartOrientation_->push_back(0);
        flightStartOrientation_->push_back(0);
      }
    } else if (key == "flightStartPosition") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        flightStartPosition_ = std::make_unique<::filament::math::float3>(
            Deserialize::Format3(std::get<flutter::EncodableMap>(it.second)));

        current_zoom_radius_ = flightStartPosition_->x;
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        flightStartPosition_ =
            std::make_unique<::filament::math::float3>(0, 0, 0);
      }
    } else if (key == "fovDirection") {
      if (std::holds_alternative<std::string>(it.second)) {
        fovDirection_ = getFovForText(std::get<std::string>(it.second));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        fovDirection_ = ::filament::camutils::Fov::VERTICAL;
      }
    } else if (key == "fovDegrees") {
      if (std::holds_alternative<double>(it.second)) {
        fovDegrees_ = std::get<double>(it.second);
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        fovDegrees_ = 33;
      }
    } else if (key == "farPlane") {
      if (std::holds_alternative<double>(it.second)) {
        farPlane_ = std::get<double>(it.second);
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        farPlane_ = 5000;
      }
    } else if (key == "groundPlane") {
      groundPlane_ = std::make_unique<std::vector<float>>();
      if (std::holds_alternative<flutter::EncodableList>(it.second)) {
        auto list = std::get<flutter::EncodableList>(it.second);
        for (const auto& item : list) {
          groundPlane_->emplace_back(std::get<double>(item));
        }
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        groundPlane_->push_back(0);
        groundPlane_->push_back(0);
        groundPlane_->push_back(1);
        groundPlane_->push_back(0);
      }
    } else if (key == "mode") {
      if (std::holds_alternative<std::string>(it.second)) {
        auto modeType = std::get<std::string>(it.second);
        if (modeType == kModeAutoOrbit) {
          eCustomCameraMode_ = CustomCameraMode::AutoOrbit;
        } else if (modeType == kModeInertiaAndGestures) {
          eCustomCameraMode_ = CustomCameraMode::InertiaAndGestures;
        } else {
          mode_ = getModeForText(modeType);
        }
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        mode_ = ::filament::camutils::Mode::ORBIT;
      }
    } else if (key == "orbitHomePosition") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        orbitHomePosition_ = std::make_unique<::filament::math::float3>(
            Deserialize::Format3(std::get<flutter::EncodableMap>(it.second)));
      }
    } else if (key == "orbitSpeed") {
      if (std::holds_alternative<flutter::EncodableList>(it.second)) {
        auto list = std::get<flutter::EncodableList>(it.second);
        orbitSpeed_ = std::make_unique<std::vector<float>>();
        for (const auto& item : list) {
          orbitSpeed_->emplace_back(std::get<double>(item));
        }
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        orbitSpeed_ = std::make_unique<std::vector<float>>();
        orbitSpeed_->push_back(0.01f);
        orbitSpeed_->push_back(0.01f);
      }
    } else if (key == "scaling" && !it.second.IsNull() &&
               std::holds_alternative<flutter::EncodableList>(it.second)) {
      auto list = std::get<flutter::EncodableList>(it.second);
      scaling_ = std::make_unique<std::vector<double>>();
      for (const auto& item : list) {
        scaling_->emplace_back(std::get<double>(item));
      }
    } else if (key == "shift" && !it.second.IsNull() &&
               std::holds_alternative<flutter::EncodableList>(it.second)) {
      auto list = std::get<flutter::EncodableList>(it.second);
      shift_ = std::make_unique<std::vector<double>>();
      for (const auto& item : list) {
        shift_->emplace_back(std::get<double>(item));
      }
    } else if (key == "targetPosition") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        targetPosition_ = std::make_unique<::filament::math::float3>(
            Deserialize::Format3(std::get<flutter::EncodableMap>(it.second)));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        targetPosition_ = std::make_unique<::filament::math::float3>(0, 0, 0);
      }
    } else if (key == "upVector") {
      if (std::holds_alternative<flutter::EncodableMap>(it.second)) {
        upVector_ = std::make_unique<::filament::math::float3>(
            Deserialize::Format3(std::get<flutter::EncodableMap>(it.second)));
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        upVector_ = std::make_unique<::filament::math::float3>(0, 1, 0);
      }
    } else if (key == "zoomSpeed") {
      if (std::holds_alternative<double>(it.second)) {
        zoomSpeed_ = std::get<double>(it.second);
      } else if (std::holds_alternative<std::monostate>(it.second)) {
        zoomSpeed_ = 0.01;
      }
    } /*else if (!it.second.IsNull()) {
      spdlog::debug("[Camera] Unhandled Parameter");
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(),
                                                           it.second);
    }*/
  }
  SPDLOG_TRACE("--Camera::Camera");
}

void Camera::Print(const char* tag) {
  spdlog::debug("++++++++");
  spdlog::debug("{} (Camera)", tag);
  if (exposure_) {
    exposure_->Print("\texposure");
  }
  if (projection_) {
    projection_->Print("\tprojection");
  }
  if (lensProjection_) {
    lensProjection_->Print("\tlensProjection");
  }
  if (farPlane_.has_value()) {
    spdlog::debug("\tfarPlane: {}", farPlane_.value());
  }
  if (flightMaxMoveSpeed_.has_value()) {
    spdlog::debug("\tflightMaxMoveSpeed: {}", flightMaxMoveSpeed_.value());
  }
  if (flightMoveDamping_.has_value()) {
    spdlog::debug("\tflightMoveDamping: {}", flightMoveDamping_.value());
  }
  if (flightSpeedSteps_.has_value()) {
    spdlog::debug("\tflightSpeedSteps: {}", flightSpeedSteps_.value());
  }
  if (flightStartOrientation_) {
    for (const auto& it_ : *flightStartOrientation_) {
      spdlog::debug("\tflightStartOrientation: {}", it_);
    }
  }
#if 0
  if (flightStartPosition_) {
    flightStartPosition_->Print("\tflightStartPosition");
  }
#endif
  if (fovDegrees_.has_value()) {
    spdlog::debug("\tfovDegrees: {}", fovDegrees_.value());
  }
  if (fovDegrees_.has_value()) {
    spdlog::debug("\tfovDegrees: {}", fovDegrees_.value());
  }
  if (farPlane_.has_value()) {
    spdlog::debug("\tfarPlane: {}", farPlane_.value());
  }
  if (groundPlane_) {
    for (const auto& it_ : *groundPlane_) {
      spdlog::debug("\tgroundPlane: {}", it_);
    }
  }

  spdlog::debug("\tmode: [{}]", getTextForMode(mode_));
#if 0
  if (orbitHomePosition_) {
    orbitHomePosition_->Print("\torbitHomePosition");
  }
#endif
  spdlog::debug("\tfovDirection: [{}]", getTextForFov(fovDirection_));
  if (orbitSpeed_) {
    for (const auto& it_ : *orbitSpeed_) {
      spdlog::debug("\torbitSpeed: {}", it_);
    }
  }
  if (scaling_) {
    for (const auto& it_ : *scaling_) {
      spdlog::debug("\tscaling: {}", it_);
    }
  }
  if (shift_) {
    for (const auto& it_ : *shift_) {
      spdlog::debug("\tshift: {}", it_);
    }
  }
#if 0
  if (targetPosition_) {
    targetPosition_->Print("\ttargetPosition");
  }
  if (upVector_) {
    upVector_->Print("\tupVector");
  }
#endif
  if (zoomSpeed_.has_value()) {
    spdlog::debug("\tzoomSpeed: {}", zoomSpeed_.value());
  }
  spdlog::debug("++++++++");
}

const char* Camera::getTextForMode(::filament::camutils::Mode mode) {
  return (const char*[]){
      kModeOrbit,
      kModeMap,
      kModeFreeFlight,
  }[static_cast<int>(mode)];
}

::filament::camutils::Mode Camera::getModeForText(const std::string& mode) {
  if (mode == kModeMap) {
    return ::filament::camutils::Mode::MAP;
  }
  if (mode == kModeFreeFlight) {
    return ::filament::camutils::Mode::FREE_FLIGHT;
  }
  return ::filament::camutils::Mode::ORBIT;
}

const char* Camera::getTextForFov(::filament::camutils::Fov fov) {
  return (const char*[]){
      kFovVertical,
      kFovHorizontal,
  }[static_cast<int>(fov)];
}

::filament::camutils::Fov Camera::getFovForText(const std::string& fov) {
  if (fov == kFovVertical) {
    return ::filament::camutils::Fov::VERTICAL;
  }

  if (fov == kFovHorizontal) {
    return ::filament::camutils::Fov::HORIZONTAL;
  }
  return ::filament::camutils::Fov::HORIZONTAL;
}
}  // namespace plugin_filament_view
