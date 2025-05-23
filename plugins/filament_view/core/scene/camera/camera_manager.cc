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

#include "camera_manager.h"
#include "touch_pair.h"

#include <core/include/additionalmath.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/transform_system.h>
#include <core/systems/derived/view_target_system.h>
#include <core/systems/ecs.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament/math/TMatHelpers.h>
#include <filament/math/mat4.h>
#include <filament/math/vec4.h>
#include <plugins/common/common.h>

#define USING_CAM_MANIPULATOR 0

namespace plugin_filament_view {

////////////////////////////////////////////////////////////////////////////
CameraManager::CameraManager(ViewTarget* poOwner)
    : currentVelocity_(0), initialTouchPosition_(0), m_poOwner(poOwner) {
  SPDLOG_TRACE("++CameraManager::CameraManager");
  setDefaultFilamentCamera();
  SPDLOG_TRACE("--CameraManager::CameraManager: {}");
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::setDefaultFilamentCamera() {
  SPDLOG_TRACE("++{}", __FUNCTION__);

  auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(
      "CameraManager::setDefaultCamera");
  const auto engine = filamentSystem->getFilamentEngine();

  auto fview = m_poOwner->getFilamentView();
  assert(fview);

  cameraEntity_ = engine->getEntityManager().create();
  camera_ = engine->createCamera(cameraEntity_);

  /// With the default parameters, the scene must contain at least one Light
  /// of intensity similar to the sun (e.g.: a 100,000 lux directional light).
  camera_->setExposure(kAperture, kShutterSpeed, kSensitivity);

  auto viewport = fview->getViewport();
  cameraManipulator_ = CameraManipulator::Builder()
                           .viewport(static_cast<int>(viewport.width),
                                     static_cast<int>(viewport.height))
                           .build(filament::camutils::Mode::ORBIT);
  filament::math::float3 eye, center, up;
  cameraManipulator_->getLookAt(&eye, &center, &up);
  setCameraLookat(eye, center, up);
  fview->setCamera(camera_);
  SPDLOG_TRACE("--{}", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::setCameraLookat(filament::math::float3 eye,
                                    filament::math::float3 center,
                                    filament::math::float3 up) const {
  if (camera_ == nullptr) {
    SPDLOG_DEBUG("Unable to set Camera Lookat, camera is null {} {}",
                 __FUNCTION__, __LINE__);
    return;
  }

  camera_->lookAt(eye, center, up);
}

////////////////////////////////////////////////////////////////////////////
std::string CameraManager::updateExposure(Exposure* exposure) const {
  if (!exposure) {
    return "Exposure not found";
  }
  const auto e = exposure;
  if (e->exposure_.has_value()) {
    SPDLOG_DEBUG("[setExposure] exposure: {}", e->exposure_.value());
    camera_->setExposure(e->exposure_.value());
    return "Exposure updated successfully";
  }

  auto aperture = e->aperture_.has_value() ? e->aperture_.value() : kAperture;
  auto shutterSpeed =
      e->shutterSpeed_.has_value() ? e->shutterSpeed_.value() : kShutterSpeed;
  auto sensitivity =
      e->sensitivity_.has_value() ? e->sensitivity_.value() : kSensitivity;
  SPDLOG_DEBUG("[setExposure] aperture: {}, shutterSpeed: {}, sensitivity: {}",
               aperture, shutterSpeed, sensitivity);
  camera_->setExposure(aperture, shutterSpeed, sensitivity);
  return "Exposure updated successfully";
}

////////////////////////////////////////////////////////////////////////////
bool CameraManager::updateProjection(const Projection* projection) const {
  if (!projection) {
    SPDLOG_DEBUG("Projection not found");
    return false;
  }
  const auto p = projection;
  if (p->projection_.has_value() && p->left_.has_value() &&
      p->right_.has_value() && p->top_.has_value() && p->bottom_.has_value()) {
    const auto project = p->projection_.value();
    auto left = p->left_.value();
    auto right = p->right_.value();
    auto top = p->top_.value();
    auto bottom = p->bottom_.value();
    auto near = p->near_.has_value() ? p->near_.value() : kNearPlane;
    auto far = p->far_.has_value() ? p->far_.value() : kFarPlane;
    SPDLOG_DEBUG(
        "[setProjection] left: {}, right: {}, bottom: {}, top: {}, near: {}, "
        "far: {}",
        left, right, bottom, top, near, far);
    camera_->setProjection(project, left, right, bottom, top, near, far);
    return true;
  }

  if (p->fovInDegrees_.has_value() && p->fovDirection_.has_value()) {
    auto fovInDegrees = p->fovInDegrees_.value();
    auto aspect =
        p->aspect_.has_value() ? p->aspect_.value() : calculateAspectRatio();
    auto near = p->near_.has_value() ? p->near_.value() : kNearPlane;
    auto far = p->far_.has_value() ? p->far_.value() : kFarPlane;
    const auto fovDirection = p->fovDirection_.value();
    SPDLOG_DEBUG(
        "[setProjection] fovInDegress: {}, aspect: {}, near: {}, far: {}, "
        "direction: {}",
        fovInDegrees, aspect, near, far,
        Projection::getTextForFov(fovDirection));

    camera_->setProjection(fovInDegrees, aspect, near, far, fovDirection);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////
std::string CameraManager::updateCameraShift(std::vector<double>* shift) const {
  if (!shift) {
    return "Camera shift not found";
  }
  const auto s = shift;
  if (s->size() >= 2) {
    return "Camera shift info must be provided";
  }
  SPDLOG_DEBUG("[setShift] {}, {}", s->at(0), s->at(1));
  camera_->setShift({s->at(0), s->at(1)});
  return "Camera shift updated successfully";
}

////////////////////////////////////////////////////////////////////////////
std::string CameraManager::updateCameraScaling(
    std::vector<double>* scaling) const {
  if (!scaling) {
    return "Camera scaling must be provided";
  }
  const auto s = scaling;
  if (s->size() >= 2) {
    return "Camera scaling info must be provided";
  }
  SPDLOG_DEBUG("[setScaling] {}, {}", s->at(0), s->at(1));
  camera_->setScaling({s->at(0), s->at(1)});
  return "Camera scaling updated successfully";
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::updateCameraManipulator(const Camera* cameraInfo) {
  if (!cameraInfo) {
    return;
  }

  auto manipulatorBuilder = CameraManipulator::Builder();

  if (cameraInfo->targetPosition_) {
    const auto tp = cameraInfo->targetPosition_.get();
    manipulatorBuilder.targetPosition(tp->x, tp->y, tp->z);

  } else {
    static constexpr filament::float3 kDefaultObjectPosition = {0.0f, 0.0f,
                                                                -4.0f};

    manipulatorBuilder.targetPosition(kDefaultObjectPosition.x,
                                      kDefaultObjectPosition.y,
                                      kDefaultObjectPosition.z);
  }

  if (cameraInfo->upVector_) {
    const auto upVector = cameraInfo->upVector_.get();
    manipulatorBuilder.upVector(upVector->x, upVector->y, upVector->z);
  }
  if (cameraInfo->zoomSpeed_.has_value()) {
    manipulatorBuilder.zoomSpeed(cameraInfo->zoomSpeed_.value());
  }

  if (cameraInfo->orbitHomePosition_) {
    const auto orbitHomePosition = cameraInfo->orbitHomePosition_.get();
    manipulatorBuilder.orbitHomePosition(
        orbitHomePosition->x, orbitHomePosition->y, orbitHomePosition->z);
  }

  if (cameraInfo->orbitSpeed_) {
    const auto orbitSpeed = cameraInfo->orbitSpeed_.get();
    manipulatorBuilder.orbitSpeed(orbitSpeed->at(0), orbitSpeed->at(1));
  }

  manipulatorBuilder.fovDirection(cameraInfo->fovDirection_);

  if (cameraInfo->fovDegrees_.has_value()) {
    manipulatorBuilder.fovDegrees(cameraInfo->fovDegrees_.value());
  }

  if (cameraInfo->farPlane_.has_value()) {
    manipulatorBuilder.farPlane(cameraInfo->farPlane_.value());
  }

  if (cameraInfo->flightStartPosition_) {
    const auto flightStartPosition = cameraInfo->flightStartPosition_.get();
    manipulatorBuilder.flightStartPosition(
        flightStartPosition->x, flightStartPosition->y, flightStartPosition->z);
  }

  if (cameraInfo->flightStartOrientation_) {
    const auto flightStartOrientation =
        cameraInfo->flightStartOrientation_.get();
    const auto pitch = flightStartOrientation->at(0);  // 0f;
    const auto yaw = flightStartOrientation->at(1);    // 0f;
    manipulatorBuilder.flightStartOrientation(pitch, yaw);
  }

  if (cameraInfo->flightMoveDamping_.has_value()) {
    manipulatorBuilder.flightMoveDamping(
        cameraInfo->flightMoveDamping_.value());
  }

  if (cameraInfo->flightSpeedSteps_.has_value()) {
    manipulatorBuilder.flightSpeedSteps(cameraInfo->flightSpeedSteps_.value());
  }

  if (cameraInfo->flightMaxMoveSpeed_.has_value()) {
    manipulatorBuilder.flightMaxMoveSpeed(
        cameraInfo->flightMaxMoveSpeed_.value());
  }

  auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(
      "CameraManager::setDefaultCamera");

  const auto viewport = m_poOwner->getFilamentView()->getViewport();
  manipulatorBuilder.viewport(static_cast<int>(viewport.width),
                              static_cast<int>(viewport.height));
  cameraManipulator_ = manipulatorBuilder.build(cameraInfo->mode_);
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::updateCamera(const Camera* cameraInfo) {
  SPDLOG_DEBUG("++CameraManager::updateCamera");

  updateExposure(cameraInfo->exposure_.get());
  // ReSharper disable once CppExpressionWithoutSideEffects
  updateProjection(cameraInfo->projection_.get());
  updateLensProjection(cameraInfo->lensProjection_.get());
  updateCameraShift(cameraInfo->shift_.get());
  updateCameraScaling(cameraInfo->scaling_.get());
  updateCameraManipulator(cameraInfo);

  SPDLOG_DEBUG("--CameraManager::updateCamera");
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::setPrimaryCamera(std::unique_ptr<Camera> camera) {
  primaryCamera_ = std::shared_ptr<Camera>(std::move(camera));

  // We'll want to set the 'defaults' depending on camera mode here.
  if (primaryCamera_->eCustomCameraMode_ == Camera::InertiaAndGestures) {
    filament::math::float3 eye, center, up;
    cameraManipulator_->getLookAt(&eye, &center, &up);

    setCameraLookat(*primaryCamera_->flightStartPosition_,
                    *primaryCamera_->targetPosition_,
                    *primaryCamera_->upVector_);
  }
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::vResetInertiaCameraToDefaultValues() {
  if (primaryCamera_->eCustomCameraMode_ == Camera::InertiaAndGestures) {
    primaryCamera_->vResetInertiaCameraToDefaultValues();

    currentVelocity_ = {0};

    setCameraLookat(*primaryCamera_->flightStartPosition_,
                    *primaryCamera_->targetPosition_,
                    *primaryCamera_->upVector_);
  }
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::lookAtDefaultPosition() const {
  filament::math::float3 eye, center, up;
  cameraManipulator_->getLookAt(&eye, &center, &up);
  setCameraLookat(eye, center, up);
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::ChangePrimaryCameraMode(const std::string& szValue) const {
  if (szValue == Camera::kModeAutoOrbit) {
    primaryCamera_->eCustomCameraMode_ = Camera::AutoOrbit;
  } else if (szValue == Camera::kModeInertiaAndGestures) {
    primaryCamera_->eCustomCameraMode_ = Camera::InertiaAndGestures;
  } else {
    spdlog::warn(
        "Camera mode unset, you tried to set to {} , but that's not "
        "implemented.",
        szValue);
    primaryCamera_->eCustomCameraMode_ = Camera::Unset;
  }
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::updateCamerasFeatures(float fElapsedTime) {
  if (!primaryCamera_ || (primaryCamera_->eCustomCameraMode_ == Camera::Unset &&
                          !primaryCamera_->forceSingleFrameUpdate_)) {
    return;
  }

  if (primaryCamera_->eCustomCameraMode_ == Camera::AutoOrbit) {
    primaryCamera_->forceSingleFrameUpdate_ = false;

    // Note these TODOs are marked for a next iteration tasking.

    // TODO this should be moved to a property on camera
    constexpr float speed = 0.5f;  // Rotation speed
    // TODO this should be moved to a property on camera
    constexpr float radius = 8.0f;  // Distance from the camera to the object

    // camera needs angle
    primaryCamera_->fCurrentOrbitAngle_ += fElapsedTime * speed;

    filament::math::float3 center = *primaryCamera_->targetPosition_;

    filament::math::float3 eye;
    eye.x = center.x + radius * std::cos(primaryCamera_->fCurrentOrbitAngle_);
    eye.y = center.y + primaryCamera_->orbitHomePosition_->y;
    eye.z = center.z + radius * std::sin(primaryCamera_->fCurrentOrbitAngle_);

    // Up vector
    filament::math::float3 up = *primaryCamera_->upVector_;

    setCameraLookat(eye, center, up);
  } else if (primaryCamera_->eCustomCameraMode_ == Camera::InertiaAndGestures) {
    currentVelocity_.y = 0.0f;

    // Update camera position around the center
    if (currentVelocity_.x == 0.0f && currentVelocity_.y == 0.0f &&
        currentVelocity_.z == 0.0f && !isPanGesture() &&
        !primaryCamera_->forceSingleFrameUpdate_) {
      return;
    }

    primaryCamera_->forceSingleFrameUpdate_ = false;

#if USING_CAM_MANIPULATOR == 0  // Not using camera manipulator
    auto rotationSpeed =
        static_cast<float>(primaryCamera_->inertia_rotationSpeed_);

    // Calculate rotation angles from velocity
    float angleX = currentVelocity_.x * rotationSpeed;
    // float angleY = currentVelocity_.y * rotationSpeed;

    // Update the orbit angle of the camera
    primaryCamera_->fCurrentOrbitAngle_ += angleX;

    // Calculate the new camera eye position based on the orbit angle
    float zoomSpeed = primaryCamera_->zoomSpeed_.value_or(0.1f);
    float radius =
        primaryCamera_->current_zoom_radius_ - currentVelocity_.z * zoomSpeed;

    // Clamp the radius between zoom_minCap_ and zoom_maxCap_
    radius =
        std::clamp(radius, static_cast<float>(primaryCamera_->zoom_minCap_),
                   static_cast<float>(primaryCamera_->zoom_maxCap_));

    filament::math::float3 center = *primaryCamera_->targetPosition_;

    filament::math::float3 eye;
    eye.x = center.x + radius * std::cos(primaryCamera_->fCurrentOrbitAngle_);
    eye.y = center.y + primaryCamera_->flightStartPosition_->y;
    eye.z = center.z + radius * std::sin(primaryCamera_->fCurrentOrbitAngle_);

    filament::math::float3 up = {0.0f, 1.0f, 0.0f};

    setCameraLookat(eye, center, up);

    // Now we're going to add on pan
    auto modelMatrix = camera_->getModelMatrix();

    auto pitchQuat = filament::math::quatf::fromAxisAngle(
        filament::float3{1.0f, 0.0f, 0.0f},
        primaryCamera_->current_pitch_addition_);

    auto yawQuat = filament::math::quatf::fromAxisAngle(
        filament::float3{0.0f, 1.0f, 0.0f},
        primaryCamera_->current_yaw_addition_);

    filament::math::mat4f pitchMatrix(pitchQuat);
    filament::math::mat4f yawMatrix(yawQuat);

    modelMatrix = modelMatrix * yawMatrix * pitchMatrix;
    camera_->setModelMatrix(modelMatrix);

#else  // using camera manipulator
    // At this time, this does not use velocity/inertia and doesn't cap Y
    // meaning you can get a full up/down view and around.
    cameraManipulator_->update(fElapsedTime);

    filament::math::float3 eye, center, up;
    cameraManipulator_->getLookAt(&eye, &center, &up);
    setCameraLookat(eye, center, up);
#endif

    // Apply inertia decay to gradually reduce velocity
    auto inertiaDecayFactor_ =
        static_cast<float>(primaryCamera_->inertia_decayFactor_);
    currentVelocity_ *= inertiaDecayFactor_;

    primaryCamera_->current_zoom_radius_ = radius;
  }
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::destroyCamera() const {
  SPDLOG_DEBUG("++CameraManager::destroyCamera");
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>("destroyCamera");
  const auto engine = filamentSystem->getFilamentEngine();

  engine->destroyCameraComponent(cameraEntity_);
  SPDLOG_DEBUG("--CameraManager::destroyCamera");
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::endGesture() {
  tentativePanEvents_.clear();
  tentativeOrbitEvents_.clear();
  tentativeZoomEvents_.clear();
  currentGesture_ = Gesture::NONE;
  cameraManipulator_->grabEnd();
}

////////////////////////////////////////////////////////////////////////////
bool CameraManager::isOrbitGesture() const {
  return tentativeOrbitEvents_.size() > kGestureConfidenceCount;
}

////////////////////////////////////////////////////////////////////////////
bool CameraManager::isPanGesture() const {
  if (tentativePanEvents_.size() <= kGestureConfidenceCount) {
    return false;
  }
  const auto oldest = tentativePanEvents_.front().midpoint();
  const auto newest = tentativePanEvents_.back().midpoint();
  return distance(oldest, newest) > kPanConfidenceDistance;
}

////////////////////////////////////////////////////////////////////////////
bool CameraManager::isZoomGesture() {
  if (tentativeZoomEvents_.size() <= kGestureConfidenceCount) {
    return false;
  }
  const auto oldest = tentativeZoomEvents_.front().separation();
  const auto newest = tentativeZoomEvents_.back().separation();
  return std::abs(newest - oldest) > kZoomConfidenceDistance;
}

////////////////////////////////////////////////////////////////////////////
Ray CameraManager::oGetRayInformationFromOnTouchPosition(
    TouchPair touch) const {
  auto [fst, snd] = aGetRayInformationFromOnTouchPosition(touch);
  constexpr float defaultLength = 1000.0f;
  Ray returnRay(fst, snd, defaultLength);
  return returnRay;
}

////////////////////////////////////////////////////////////////////////////
std::pair<filament::math::float3, filament::math::float3>
CameraManager::aGetRayInformationFromOnTouchPosition(TouchPair touch) const {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "CameraManager::aGetRayInformationFromOnTouchPosition");

  const auto viewport = m_poOwner->getFilamentView()->getViewport();

  // Note at time of writing on a 800*600 resolution this seems like the 10%
  // edges aren't super accurate this might need to be looked at more.

  float ndcX = (2.0f * static_cast<float>(touch.x())) /  // NOLINT
                   static_cast<float>(viewport.width) -  // NOLINT
               1.0f;
  float ndcY = 1.0f - (2.0f * static_cast<float>(touch.y())) /  // NOLINT
                          static_cast<float>(viewport.height);  // NOLINT
  ndcY = -ndcY;

  filament::math::vec4<float> rayClip(ndcX, ndcY, -1.0f, 1.0f);

  // Get inverse projection and view matrices
  filament::math::mat4 invProj = inverse(camera_->getProjectionMatrix());
  filament::math::vec4<double> rayView = invProj * rayClip;
  rayView = filament::math::vec4<double>(rayView.x, rayView.y, -1.0f, 0.0f);

  filament::math::mat4 invView = inverse(camera_->getViewMatrix());
  filament::math::vec3<double> rayDirection = (invView * rayView).xyz;
  rayDirection = normalize(rayDirection);

  // Camera position
  filament::math::vec3<double> rayOrigin = invView[3].xyz;

  return {rayOrigin, rayDirection};
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::onAction(int32_t action,
                             const int32_t point_count,
                             const size_t point_data_size,
                             const double* point_data) {
  // We only care about updating the camera on action if we're set to use those
  // values.
  if (primaryCamera_ == nullptr ||
      primaryCamera_->eCustomCameraMode_ != Camera::InertiaAndGestures ||
      cameraManipulator_ == nullptr) {
    return;
  }

#if 0  // Hack testing code - for testing camera controls on PC
  if ( action == ACTION_DOWN || action == ACTION_MOVE) {
    currentVelocity_.z += 1.0f;
    return;
  } else if (action == ACTION_UP) {
    currentVelocity_.z -= 1.0f;
    return;
  }
#endif

  auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(
      "CameraManager::setDefaultCamera");

  const auto viewport = m_poOwner->getFilamentView()->getViewport();
  auto touch =
      TouchPair(point_count, point_data_size, point_data, viewport.height);
  switch (action) {
    case ACTION_DOWN: {
      if (point_count == 1) {
        cameraManipulator_->grabBegin(touch.x(), touch.y(), false);
        initialTouchPosition_ = {touch.x(), touch.y()};
        currentVelocity_ = {0.0f};
      }
    } break;

    case ACTION_MOVE: {
      // CANCEL GESTURE DUE TO UNEXPECTED POINTER COUNT
      if ((point_count != 1 && currentGesture_ == Gesture::ORBIT) ||
          (point_count != 2 && currentGesture_ == Gesture::PAN) ||
          (point_count != 2 && currentGesture_ == Gesture::ZOOM)) {
        endGesture();
        return;
      }

      // UPDATE EXISTING GESTURE

      if (currentGesture_ == Gesture::ZOOM) {
        const auto d0 = previousTouch_.separation();
        const auto d1 = touch.separation();
        cameraManipulator_->scroll(touch.x(), touch.y(),
                                   (d0 - d1) * kZoomSpeed);

        currentVelocity_.z = (d0 - d1) * kZoomSpeed;

        previousTouch_ = touch;
        return;
      }

      if (currentGesture_ != Gesture::NONE) {
        cameraManipulator_->grabUpdate(touch.x(), touch.y());
        if (isPanGesture()) {
          return;
        }
      }

      // DETECT NEW GESTURE
      if (point_count == 1) {
        tentativeOrbitEvents_.push_back(touch);
      }

      if (point_count == 2) {
        tentativePanEvents_.push_back(touch);
        tentativeZoomEvents_.push_back(touch);
      }

      // Calculate the delta movement
      const filament::math::float2 currentPosition = {touch.x(), touch.y()};
      const filament::math::float2 delta =
          currentPosition - initialTouchPosition_;

      const auto velocityFactor =
          static_cast<float>(primaryCamera_->inertia_velocityFactor_);

      if (isOrbitGesture()) {
        cameraManipulator_->grabUpdate(touch.x(), touch.y());
        currentGesture_ = Gesture::ORBIT;

        // Update velocity based on movement
        currentVelocity_.xy += delta * velocityFactor;

        // Update touch position for the next move
        initialTouchPosition_ = currentPosition;

        return;
      }

      if (isZoomGesture()) {
        currentGesture_ = Gesture::ZOOM;
        previousTouch_ = touch;
        return;
      }

      if (isPanGesture()) {
        primaryCamera_->current_pitch_addition_ +=
            delta.y * velocityFactor * .01f;
        primaryCamera_->current_yaw_addition_ -=
            delta.x * velocityFactor * .01f;

        // Convert your angle caps from degrees to radians
        const float pitchCapRadians =
            static_cast<float>(primaryCamera_->pan_angleCapX_) *
            degreesToRadians;
        const float yawCapRadians =
            static_cast<float>(primaryCamera_->pan_angleCapY_) *
            degreesToRadians;

        primaryCamera_->current_pitch_addition_ =
            std::clamp(primaryCamera_->current_pitch_addition_,
                       -pitchCapRadians, pitchCapRadians);

        primaryCamera_->current_yaw_addition_ =
            std::clamp(primaryCamera_->current_yaw_addition_, -yawCapRadians,
                       yawCapRadians);

        cameraManipulator_->grabBegin(touch.x(), touch.y(), true);
        currentGesture_ = Gesture::PAN;
      }
    } break;
    case ACTION_CANCEL:
    case ACTION_UP:
    default:
      endGesture();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////
std::string CameraManager::updateLensProjection(
    const LensProjection* lensProjection) {
  if (!lensProjection) {
    return "Lens projection not found";
  }

  const float lensProjectionFocalLength = lensProjection->getFocalLength();
  if (cameraFocalLength_ != lensProjectionFocalLength)
    cameraFocalLength_ = lensProjectionFocalLength;
  const auto aspect = lensProjection->getAspect().has_value()
                          ? lensProjection->getAspect().value()
                          : calculateAspectRatio();
  camera_->setLensProjection(
      lensProjectionFocalLength, aspect,
      lensProjection->getNear().has_value() ? lensProjection->getNear().value()
                                            : kNearPlane,
      lensProjection->getFar().has_value() ? lensProjection->getFar().value()
                                           : kFarPlane);
  return "Lens projection updated successfully";
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::updateCameraProjection() {
  const auto aspect = calculateAspectRatio();
  const auto lensProjection = new LensProjection(cameraFocalLength_, aspect);
  updateLensProjection(lensProjection);
  delete lensProjection;
}

////////////////////////////////////////////////////////////////////////////
float CameraManager::calculateAspectRatio() const {
  auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(
      "CameraManager::aGetRayInformationFromOnTouchPosition");

  const auto viewport = m_poOwner->getFilamentView()->getViewport();
  return static_cast<float>(viewport.width) /
         static_cast<float>(viewport.height);
}

////////////////////////////////////////////////////////////////////////////
void CameraManager::updateCameraOnResize(const uint32_t width,
                                         const uint32_t height) {
  cameraManipulator_->setViewport(static_cast<int>(width),
                                  static_cast<int>(height));
  updateCameraProjection();
}

}  // namespace plugin_filament_view
