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

#include "view_target.h"

#include <core/include/additionalmath.h>
#include <core/include/literals.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/transform_system.h>
#include <core/systems/derived/view_target_system.h>
#include <core/systems/ecs.h>

#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament/math/TMatHelpers.h>
#include <filament/math/mat4.h>
#include <filament/math/vec4.h>
#include <filament/utils/EntityManager.h>

#include <asio/post.hpp>
#include <flutter/encodable_value.h>
#include <gltfio/Animator.h>
#include <plugins/common/common.h>
#include <utility>
#include <view/flutter_view.h>
#include <wayland/display.h>

using flutter::EncodableList;
using flutter::EncodableMap;
using flutter::EncodableValue;
using flutter::MessageReply;
using flutter::MethodCall;
using flutter::MethodResult;

class Display;
class FlutterView;
class FilamentViewPlugin;

namespace plugin_filament_view {
////////////////////////////////////////////////////////////////////////////
ViewTarget::ViewTarget(
  const size_t id,
  const int32_t top,
  const int32_t left,
  FlutterDesktopEngineState* state
)
  : id(id),
    state_(state),
    left_(left),
    top_(top),
    callback_(nullptr),
    fanimator_(nullptr) {
  /* Setup Wayland subsurface */
  setupWaylandSubsurface();
}

////////////////////////////////////////////////////////////////////////////
ViewTarget::~ViewTarget() {
  _engine->destroyCameraComponent(cameraEntity_);

  SPDLOG_TRACE("++{}", __FUNCTION__);

  if (callback_) {
    wl_callback_destroy(callback_);
    callback_ = nullptr;
  }

  _engine->destroy(fview_);
  _engine->destroy(fswapChain_);

  if (subsurface_) {
    wl_subsurface_destroy(subsurface_);
    subsurface_ = nullptr;
  }

  if (surface_) {
    wl_surface_destroy(surface_);
    surface_ = nullptr;
  }
  SPDLOG_TRACE("--{}", __FUNCTION__);
}

void ViewTarget::_initCamera() {
  runtime_assert(fview_, "Filament view must be initialized before setting up the camera");

  cameraEntity_ = _engine->getEntityManager().create();
  camera_ = _engine->createCamera(cameraEntity_);

  // Set defaults
  camera_->setExposure(kDefaultAperture, kDefaultShutterSpeed, kDefaultSensitivity);
  camera_->setModelMatrix(filament::math::mat4f());
  fview_->setCamera(camera_);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::setupWaylandSubsurface() {
  // Ensure state_ is properly initialized
  if (!state_ || !state_->view_controller) {
    // Handle error: state_ or view_controller is not initialized
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    return;
  }

  const auto flutter_view = state_->view_controller->view;
  if (!flutter_view) {
    // Handle error: flutter_view is not initialized
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    return;
  }

  display_ = flutter_view->GetDisplay()->GetDisplay();
  if (!display_) {
    // Handle error: display is not initialized
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    return;
  }

  parent_surface_ = flutter_view->GetWindow()->GetBaseSurface();
  if (!parent_surface_) {
    // Handle error: parent_surface is not initialized
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    return;
  }

  surface_ = wl_compositor_create_surface(flutter_view->GetDisplay()->GetCompositor());
  if (!surface_) {
    // Handle error: failed to create surface
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    return;
  }

  subsurface_ = wl_subcompositor_get_subsurface(
    flutter_view->GetDisplay()->GetSubCompositor(), surface_, parent_surface_
  );
  if (!subsurface_) {
    // Handle error: failed to create subsurface
    spdlog::error("{}::{}", __FUNCTION__, __LINE__);
    wl_surface_destroy(surface_);  // Clean up the surface
    return;
  }

  wl_subsurface_place_below(subsurface_, parent_surface_);
  wl_subsurface_set_desync(subsurface_);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::InitializeFilamentInternals(const uint32_t width, const uint32_t height) {
  SPDLOG_TRACE("++{}", __FUNCTION__);

  native_window_ = {.display = display_, .surface = surface_, .width = width, .height = height};

  const auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(
    "ViewTarget::Initialize"
  );

  _engine = filamentSystem->getFilamentEngine();
  fswapChain_ = _engine->createSwapChain(&native_window_);
  fview_ = _engine->createView();

  setupView(width, height);

  SPDLOG_TRACE("--{}", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::setupView(uint32_t width, uint32_t height) {
  SPDLOG_TRACE("++{}", __FUNCTION__);

  const auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>(__FUNCTION__);

  fview_->setScene(filamentSystem->getFilamentScene());

  // this probably needs to change
  fview_->setVisibleLayers(0x4, 0x4);
  fview_->setViewport({0, 0, width, height});

  fview_->setBlendMode(filament::View::BlendMode::TRANSLUCENT);

  // on mobile, better use lower quality color buffer
  // filament::View::RenderQuality renderQuality{};
  // renderQuality.hdrColorBuffer = filament::View::QualityLevel::MEDIUM;
  // fview_->setRenderQuality(renderQuality);

  // dynamic resolution often helps a lot
  // fview_->setDynamicResolutionOptions(
  //     {.enabled = true, .quality = filament::View::QualityLevel::MEDIUM});

  // MSAA is needed with dynamic resolution MEDIUM
  // fview_->setMultiSampleAntiAliasingOptions({.enabled = true});
  // fview_->setMultiSampleAntiAliasingOptions({.enabled = false});

  // FXAA is pretty economical and helps a lot
  // fview_->setAntiAliasing(filament::View::AntiAliasing::FXAA);
  // fview_->setAntiAliasing(filament::View::AntiAliasing::NONE);

  // ambient occlusion is the cheapest effect that adds a lot of quality
  fview_->setAmbientOcclusionOptions({.enabled = true, .ssct = {}});
  // fview_->setAmbientOcclusion(filament::View::AmbientOcclusion::NONE);

  // fview_->setShadowingEnabled(false);
  // fview_->setScreenSpaceRefractionEnabled(false);
  // fview_->setStencilBufferEnabled(false);
  // fview_->setDynamicLightingOptions(0.01, 1000.0f);

  vChangeQualitySettings(ViewTarget::ePredefinedQualitySettings::Ultra);

  _initCamera();

  SPDLOG_TRACE("--{}", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::vChangeQualitySettings(const ePredefinedQualitySettings qualitySettings) {
  // Reset the settings for each quality setting
  filament::viewer::ViewSettings settings = settings_.view;

  switch (qualitySettings) {
    case Lowest:
      settings.antiAliasing = filament::View::AntiAliasing::NONE;
      settings.msaa.enabled = false;
      settings.dsr = {.enabled = false};
      settings.screenSpaceReflections.enabled = false;
      settings.bloom.enabled = false;
      settings.postProcessingEnabled = false;
      settings.dynamicLighting.zLightNear = 0.01f;
      settings.dynamicLighting.zLightFar = 50.0f;
      settings.shadowType = filament::View::ShadowType::PCF;
      settings.renderQuality = {.hdrColorBuffer = filament::View::QualityLevel::LOW};
      fview_->setStencilBufferEnabled(false);
      fview_->setScreenSpaceRefractionEnabled(false);
      break;

    case Low:
      settings.antiAliasing = filament::View::AntiAliasing::FXAA;
      settings.msaa.enabled = false;
      settings.dsr = {.enabled = false};
      settings.screenSpaceReflections.enabled = false;
      settings.bloom = {.enabled = false};
      settings.postProcessingEnabled = true;
      settings.dynamicLighting.zLightNear = 0.01f;
      settings.dynamicLighting.zLightFar = 100.0f;
      settings.shadowType = filament::View::ShadowType::PCF;
      settings.renderQuality = {.hdrColorBuffer = filament::View::QualityLevel::LOW};
      fview_->setStencilBufferEnabled(true);
      fview_->setScreenSpaceRefractionEnabled(false);
      break;

    case Medium:
      settings.antiAliasing = filament::View::AntiAliasing::FXAA;
      settings.msaa.enabled = false;
      settings.dsr = {.enabled = false};
      settings.screenSpaceReflections.enabled = false;
      settings.bloom = {.strength = 0.2f, .enabled = true};
      settings.postProcessingEnabled = true;
      settings.dynamicLighting.zLightNear = 0.01f;
      settings.dynamicLighting.zLightFar = 250.0f;
      settings.shadowType = filament::View::ShadowType::PCF;
      settings.renderQuality = {.hdrColorBuffer = filament::View::QualityLevel::HIGH};
      fview_->setStencilBufferEnabled(true);
      fview_->setScreenSpaceRefractionEnabled(true);
      break;

    case High:
      settings.antiAliasing = filament::View::AntiAliasing::FXAA;
      settings.msaa.enabled = true;
      settings.msaa.sampleCount = 2;
      settings.dsr = {.enabled = false};
      settings.screenSpaceReflections = {
        .thickness = 0.05f, .bias = 0.5f, .maxDistance = 4.0f, .stride = 2.0f, .enabled = true
      };
      settings.bloom = {.strength = 0.3f, .enabled = true};
      settings.postProcessingEnabled = true;
      settings.dynamicLighting.zLightNear = 0.01f;
      settings.dynamicLighting.zLightFar = 500.0f;
      settings.shadowType = filament::View::ShadowType::PCF;
      settings.renderQuality = {.hdrColorBuffer = filament::View::QualityLevel::HIGH};
      fview_->setStencilBufferEnabled(true);
      fview_->setScreenSpaceRefractionEnabled(true);
      break;

    case Ultra:
      settings.antiAliasing = filament::View::AntiAliasing::FXAA;
      settings.msaa.enabled = true;
      settings.msaa.sampleCount = 4;
      settings.dsr = {.enabled = false};
      settings.screenSpaceReflections = {
        .thickness = 0.05f, .bias = 0.5f, .maxDistance = 4.0f, .stride = 2.0f, .enabled = true
      };
      settings.bloom = {.strength = 0.4f, .enabled = true};
      settings.postProcessingEnabled = true;
      settings.dynamicLighting.zLightNear = 0.01f;
      settings.dynamicLighting.zLightFar = 1000.0f;
      settings.renderQuality = {.hdrColorBuffer = filament::View::QualityLevel::HIGH};
      settings.shadowType = filament::View::ShadowType::PCF;
      fview_->setStencilBufferEnabled(true);
      fview_->setScreenSpaceRefractionEnabled(true);
      break;
  }

  // Now apply the settings to the Filament engine and view
  applySettings(_engine, settings, fview_);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::vSetFogOptions(const filament::View::FogOptions& fogOptions) {
  fview_->setFogOptions(fogOptions);
}

void ViewTarget::updateCameraSettings(Camera& cameraData, BaseTransform& transform) {
  // spdlog::debug("Updating camera({}) for view({})", cameraData.GetOwner()->GetGuid(), id);

  // Update transform
  // Calculate matrices
  auto headMatrix =
    // Fulcrum position - camera rotates around this point
    filament::math::mat4f::translation(transform.local.position)
    // Rig rotation - camera rig rotates around fulcrum
    * filament::math::mat4f(transform.local.rotation)
    // Dolly offset - camera is offset from the rig arm
    * filament::math::mat4f::translation(cameraData._dollyOffset);

  camera_->setModelMatrix(headMatrix);

  // Update exposure
  if (cameraData._dirtyExposure) {
    _setExposure(*cameraData.getExposure());
  }

  // Update projection
  if (cameraData._dirtyProjection) {
    if (cameraData._projection != std::nullopt) {
      _setProjection(*cameraData.getProjection());
    } else if (cameraData._lens != std::nullopt) {
      _setLens(*cameraData.getLens());
    } else {
      throw std::runtime_error("Camera projection or lens must be set before updating the camera.");
    }
  }

  // Update eyes
  // NOTE: currently disabled, unnecessary until we support stereo rendering
  // if (cameraData._dirtyEyes) {
  //   _setEyes(cameraData._ipd);
  // }

  cameraData.clearDirtyFlags();
}

void ViewTarget::_setExposure(const Exposure& e) {
  if (e.exposure_.has_value()) {
    SPDLOG_DEBUG("[setExposure] exposure: {}", e.exposure_.value());
    camera_->setExposure(e.exposure_.value());
  } else {
    auto aperture = e.aperture_.has_value() ? e.aperture_.value() : kDefaultAperture;
    auto shutterSpeed = e.shutterSpeed_.has_value() ? e.shutterSpeed_.value()
                                                    : kDefaultShutterSpeed;
    auto sensitivity = e.sensitivity_.has_value() ? e.sensitivity_.value() : kDefaultSensitivity;
    SPDLOG_DEBUG(
      "[setExposure] aperture: {}, shutterSpeed: {}, sensitivity: {}",  //
      aperture, shutterSpeed, sensitivity
    );

    camera_->setExposure(aperture, shutterSpeed, sensitivity);
  }
}

void ViewTarget::_setProjection(const Projection& p) {
  // Sets projection from raw values if present
  if (p.projection_.has_value() && p.left_.has_value() && p.right_.has_value() && p.top_.has_value()
      && p.bottom_.has_value()) {
    const auto project = p.projection_.value();
    auto left = p.left_.value();
    auto right = p.right_.value();
    auto top = p.top_.value();
    auto bottom = p.bottom_.value();
    auto near = p.near_.has_value() ? p.near_.value() : kDefaultNearPlane;
    auto far = p.far_.has_value() ? p.far_.value() : kDefaultFarPlane;
    SPDLOG_DEBUG(
      "[setProjection] left: {}, right: {}, bottom: {}, top: {}, near: {}, far: {}",  //
      left, right, bottom, top, near, far
    );
    camera_->setProjection(project, left, right, bottom, top, near, far);
  }
  // ...else calculate & set from FOV
  else if (p.fovInDegrees_.has_value() && p.fovDirection_.has_value()) {
    auto fovInDegrees = p.fovInDegrees_.value();
    auto aspect = p.aspect_.has_value() ? p.aspect_.value() : calculateAspectRatio();
    auto near = p.near_.has_value() ? p.near_.value() : kDefaultNearPlane;
    auto far = p.far_.has_value() ? p.far_.value() : kDefaultFarPlane;
    const auto fovDirection = p.fovDirection_.value();
    SPDLOG_DEBUG(
      "[setProjection] fovInDegress: {}, aspect: {}, near: {}, far: {}, direction: {}",  //
      fovInDegrees, aspect, near, far, Projection::getTextForFov(fovDirection)
    );

    camera_->setProjection(fovInDegrees, aspect, near, far, fovDirection);
  }
}

void ViewTarget::_setLens(const LensProjection& l) {
  const float focalLength = l.getFocalLength();

  const auto aspect = l.getAspect().has_value() ? l.getAspect().value() : calculateAspectRatio();

  const auto near = l.getNear().has_value() ? l.getNear().value() : kDefaultNearPlane;
  const auto far = l.getFar().has_value() ? l.getFar().value() : kDefaultFarPlane;
  SPDLOG_DEBUG(
    "[setLens] focalLength: {}, aspect: {}, near: {}, far: {}",  //
    focalLength, aspect, near, far
  );

  camera_->setLensProjection(focalLength, aspect, near, far);
}

void ViewTarget::_setEyes(const double ipd) {
  // initialized to identity
  filament::math::mat4 leftEye = {};
  filament::math::mat4 rightEye = {};

  if (ipd != 0.0f) {
    filament::math::double3 ipdOffset = {ipd / 2.0, 0.0, 0.0};

    leftEye = filament::math::mat4::translation(ipdOffset);
    rightEye = filament::math::mat4::translation(ipdOffset);
    // TODO: add support for focus distance (rotate both eyes towards the focal point)
  }

  spdlog::trace("[setEyes] ipd: {}m", ipd);
  /// TODO: to enable stereo 3D rendering, check Engine for `stereoscopicEyeCount` first
  camera_->setEyeModelMatrix(0, leftEye);
  camera_->setEyeModelMatrix(1, rightEye);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::SendFrameViewCallback(
  const std::string& methodName,
  std::initializer_list<std::pair<const char*, EncodableValue>> args
) {
  EncodableMap encodableMap;
  encodableMap.insert({EncodableValue("method"), flutter::EncodableValue(methodName)});
  for (const auto& [fst, snd] : args) {
    encodableMap[EncodableValue(fst)] = snd;  // NOLINT
  }

  const auto viewTargetSystem = ECSManager::GetInstance()->getSystem<ViewTargetSystem>(__FUNCTION__
  );

  viewTargetSystem->vSendDataToEventChannel(encodableMap);
}

/////////////////////////////////////////////////////////////////////////
const wl_callback_listener ViewTarget::frame_listener = {.done = OnFrame};

/**
 * Renders the model and updates the Filament camera.
 *
 * @param time - timestamp of running program
 * rendered
 */
void ViewTarget::DrawFrame(const uint32_t time) {
  if (m_LastTime == 0) {
    m_LastTime = time;
  }

  // Frames from Native to dart, currently run in order of
  // - updateFrame - Called regardless if a frame is going to be drawn or not
  // - preRenderFrame - Called before native <features>, but we know we're
  // going to draw a frame
  // - renderFrame - Called after native <features>, right before drawing a
  // frame
  // - postRenderFrame - Called after we've drawn natively, right after
  // drawing a frame.

  // cast time to int32_t, since EncodableValue does not support uint32_t
  int32_t s_lastTime = m_LastTime;  // NOLINT
  SendFrameViewCallback(
    kUpdateFrame, {std::make_pair(kParam_ElapsedFrameTime, EncodableValue(s_lastTime))}
  );

  // Render the scene, unless the renderer wants to skip the frame.
  if (const auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>("DrawFrame");
      filamentSystem->getFilamentRenderer()->beginFrame(fswapChain_, time)) {
    // Note you might want render time and gameplay time to be different
    // but for smooth animation you don't. (physics would be simulated w/o
    // render)
    //
    // Future tasking for making a more featured timing / frame info class.
    const uint32_t deltaTimeMS = time - m_LastTime;
    float timeSinceLastRenderedSec = static_cast<float>(deltaTimeMS)
                                     / 1000.0f;  // convert to seconds
    if (timeSinceLastRenderedSec == 0.0f) {
      timeSinceLastRenderedSec += 1.0f;
    }
    float fps = 1.0f / timeSinceLastRenderedSec;  // calculate FPS

    SendFrameViewCallback(
      kPreRenderFrame,
      {std::make_pair(kParam_TimeSinceLastRenderedSec, EncodableValue(timeSinceLastRenderedSec)),
       std::make_pair(kParam_FPS, EncodableValue(fps))}
    );

    filamentSystem->getFilamentRenderer()->render(fview_);

    filamentSystem->getFilamentRenderer()->endFrame();

    SendFrameViewCallback(
      kPostRenderFrame,
      {std::make_pair(kParam_TimeSinceLastRenderedSec, EncodableValue(timeSinceLastRenderedSec)),
       std::make_pair(kParam_FPS, EncodableValue(fps))}
    );
  }

  m_LastTime = time;
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::OnFrame(void* data, wl_callback* callback, const uint32_t time) {
  post(*ECSManager::GetInstance()->GetStrand(), [data, callback, time] {
    const auto obj = static_cast<ViewTarget*>(data);
    obj->callback_ = nullptr;

    if (callback) {
      wl_callback_destroy(callback);
    }

    obj->DrawFrame(time);

    obj->callback_ = wl_surface_frame(obj->surface_);
    wl_callback_add_listener(obj->callback_, &ViewTarget::frame_listener, data);

    // Z-Order
    // These do not need <seem> to need to be called every frame.
    // wl_subsurface_place_below(obj->subsurface_, obj->parent_surface_);
    wl_subsurface_set_position(obj->subsurface_, obj->left_, obj->top_);

    wl_surface_commit(obj->surface_);
  });
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::setOffset(const double left, const double top) {
  left_ = static_cast<int32_t>(left);
  top_ = static_cast<int32_t>(top);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::resize(const double width, const double height) {
  // Will need to determine what bottom should be
  fview_->setViewport({left_, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)});

  // Update lens projection
  const auto aspect = calculateAspectRatio();
  const auto focalLength = static_cast<float>(camera_->getFocalLength());
  const LensProjection lensProjection = {focalLength, aspect};
  _setLens(lensProjection);
}

float ViewTarget::calculateAspectRatio() const {
  const auto viewport = fview_->getViewport();
  if (viewport.height == 0) {
    return 1.0f;  // Avoid division by zero
  }
  return static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
}

////////////////////////////////////////////////////////////////////////////
void ViewTarget::vOnTouch(
  const int32_t action,
  const int32_t point_count,
  const size_t point_data_size,
  const double* point_data
) const {
  // if action is 0, then on 'first' touch, cast ray from camera;
  const auto viewport = fview_->getViewport();
  const auto touch = TouchPair(point_count, point_data_size, point_data, viewport.height);

  static constexpr int ACTION_DOWN = 0;

  if (action == ACTION_DOWN) {
    const auto rayInfo = touchToRay(touch);

    ECSMessage rayInformation;
    rayInformation.addData(ECSMessageType::DebugLine, rayInfo);
    ECSManager::GetInstance()->vRouteMessage(rayInformation);

    ECSMessage collisionRequest;
    collisionRequest.addData(ECSMessageType::CollisionRequest, rayInfo);
    collisionRequest.addData(ECSMessageType::CollisionRequestRequestor, std::string(__FUNCTION__));
    collisionRequest.addData(ECSMessageType::CollisionRequestType, eNativeOnTouchBegin);
    ECSManager::GetInstance()->vRouteMessage(collisionRequest);
  }
}

Ray ViewTarget::touchToRay(const TouchPair touch) const {
  const auto viewport = fview_->getViewport();

  // Note at time of writing on a 800*600 resolution this seems like the 10%
  // edges aren't super accurate this might need to be looked at more.
  float ndcX = (2.0f * static_cast<float>(touch.x())) /  // NOLINT
                 static_cast<float>(viewport.width)
               -                                         // NOLINT
               1.0f;
  float ndcY = 1.0f
               - (2.0f * static_cast<float>(touch.y())) /  // NOLINT
                   static_cast<float>(viewport.height);    // NOLINT
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

  /// TODO: this should be the real distance to the object
  constexpr float defaultLength = 1000.0f;
  return {
    filament::math::float3(rayOrigin.x, rayOrigin.y, rayOrigin.z),
    filament::math::float3(rayDirection.x, rayDirection.y, rayDirection.z),  //
    defaultLength
  };
}

}  // namespace plugin_filament_view
