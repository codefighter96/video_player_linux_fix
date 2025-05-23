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

namespace plugin_filament_view {

// Messages.cc usage from Dart->C++
static constexpr char kChangeAnimationByIndex[] = "CHANGE_ANIMATION_BY_INDEX";
static constexpr char kChangeLightTransformByGUID[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID";
static constexpr char kChangeLightTransformByGUIDPosx[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_POSX";
static constexpr char kChangeLightTransformByGUIDPosy[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_POSY";
static constexpr char kChangeLightTransformByGUIDPosz[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_POSZ";
static constexpr char kChangeLightTransformByGUIDDirx[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_DIRX";
static constexpr char kChangeLightTransformByGUIDDiry[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_DIRY";
static constexpr char kChangeLightTransformByGUIDDirz[] = "CHANGE_LIGHT_TRANSFORM_BY_GUID_DIRZ";

static constexpr char kChangeLightColorByGUID[] = "CHANGE_LIGHT_COLOR_BY_GUID";
static constexpr char kChangeLightColorByGUIDColor[] = "CHANGE_LIGHT_COLOR_BY_GUID_COLOR";
static constexpr char kChangeLightColorByGUIDIntensity[] = "CHANGE_LIGHT_COLOR_BY_GUID_INTENSITY";

static constexpr char kToggleShapesInScene[] = "TOGGLE_SHAPES_IN_SCENE";
static constexpr char kToggleShapesInSceneValue[] = "TOGGLE_SHAPES_IN_SCENE_VALUE";
static constexpr char kToggleCollidableVisualsInScene[] = "TOGGLE_COLLIDABLE_VISUALS_IN_SCENE";
static constexpr char kToggleCollidableVisualsInSceneValue[] =
  "TOGGLE_COLLIDABLE_VISUALS_IN_SCENE_VALUE";
static constexpr char kChangeCameraMode[] = "CHANGE_CAMERA_MODE";
static constexpr char kChangeCameraModeValue[] = "CHANGE_CAMERA_MODE_VALUE";
static constexpr char kChangeCameraRotation[] = "ROTATE_CAMERA";
static constexpr char kChangeCameraRotationValue[] = "ROTATE_CAMERA_VALUE";
static constexpr char kResetInertiaCameraToDefaultValues[] = "RESET_INERTIA_TO_DEFAULTS";
static constexpr char kChangeQualitySettings[] = "CHANGE_QUALITY_SETTINGS";

static constexpr char kAnimationEnqueue[] = "ANIMATION_ENQUEUE";
static constexpr char kAnimationClearQueue[] = "ANIMATION_CLEAR_QUEUE";
static constexpr char kAnimationPlay[] = "ANIMATION_PLAY";
static constexpr char kAnimationChangeSpeed[] = "ANIMATION_CHANGE_SPEED";
static constexpr char kAnimationChangeSpeedValue[] = "ANIMATION_CHANGE_SPEED_VALUE";
static constexpr char kAnimationPause[] = "ANIMATION_PAUSE";
static constexpr char kAnimationResume[] = "ANIMATION_RESUME";
static constexpr char kAnimationSetLooping[] = "ANIMATION_SET_LOOPING";
static constexpr char kAnimationSetLoopingValue[] = "ANIMATION_LOOPING_VALUE";
static constexpr char kEntityGUID[] = "ENTITY_GUID";

// Collision Requests
static constexpr char kCollisionRayRequest[] = "COLLISION_RAY_REQUEST";
static constexpr char kCollisionRayRequestOriginX[] = "COLLISION_RAY_REQUEST_ORIGIN_X";
static constexpr char kCollisionRayRequestOriginY[] = "COLLISION_RAY_REQUEST_ORIGIN_Y";
static constexpr char kCollisionRayRequestOriginZ[] = "COLLISION_RAY_REQUEST_ORIGIN_Z";
static constexpr char kCollisionRayRequestDirectionX[] = "COLLISION_RAY_REQUEST_DIRECTION_X";
static constexpr char kCollisionRayRequestDirectionY[] = "COLLISION_RAY_REQUEST_DIRECTION_Y";
static constexpr char kCollisionRayRequestDirectionZ[] = "COLLISION_RAY_REQUEST_DIRECTION_Z";
static constexpr char kCollisionRayRequestLength[] = "COLLISION_RAY_REQUEST_LENGTH";
static constexpr char kCollisionRayRequestGUID[] = "COLLISION_RAY_REQUEST_GUID";

static constexpr char kChangeMaterialParameter[] = "CHANGE_MATERIAL_PARAMETER";
static constexpr char kChangeMaterialParameterData[] = "CHANGE_MATERIAL_PARAMETER_DATA";
static constexpr char kChangeMaterialParameterEntityGuid[] =
  "CHANGE_MATERIAL_PARAMETER_ENTITY_GUID";

static constexpr char kChangeMaterialDefinition[] = "CHANGE_MATERIAL_DEFINITION";
static constexpr char kChangeMaterialDefinitionData[] = "CHANGE_MATERIAL_DEFINITION_DATA";
static constexpr char kChangeMaterialDefinitionEntityGuid[] =
  "CHANGE_MATERIAL_DEFINITION_ENTITY_GUID";

// Deserialization
static constexpr char kId[] = "id";
static constexpr char kParentId[] = "parentId";
static constexpr char kName[] = "name";
static constexpr char kGuid[] = "guid";
static constexpr char kShapeType[] = "shapeType";
static constexpr char kSize[] = "size";
static constexpr char kPosition[] = "position";
static constexpr char kStartingPosition[] = "startingPosition";
static constexpr char kNormal[] = "normal";
static constexpr char kScale[] = "scale";
static constexpr char kRotation[] = "rotation";
static constexpr char kStacks[] = "stacks";
static constexpr char kSlices[] = "slices";
static constexpr char kMaterial[] = "material";
static constexpr char kDoubleSided[] = "doubleSided";
static constexpr char kCullingEnabled[] = "cullingEnabled";
static constexpr char kReceiveShadows[] = "receiveShadows";
static constexpr char kCastShadows[] = "castShadows";
static constexpr char kDirection[] = "direction";
static constexpr char kLength[] = "length";
static constexpr char kModels[] = "models";
static constexpr char kFallback[] = "fallback";
static constexpr char kScene[] = "scene";
static constexpr char kShapes[] = "shapes";
static constexpr char kSkybox[] = "skybox";
static constexpr char kLight[] = "light";
static constexpr char kLights[] = "lights";
static constexpr char kIndirectLight[] = "indirectLight";
static constexpr char kCamera[] = "camera";
static constexpr char kExposure[] = "exposure";
static constexpr char kProjection[] = "projection";
static constexpr char kLensProjection[] = "lensProjection";
static constexpr char kFlightMaxMoveSpeed[] = "flightMaxMoveSpeed";
static constexpr char kFlightMoveDamping[] = "flightMoveDamping";
static constexpr char kFlightSpeedSteps[] = "flightSpeedSteps";
static constexpr char kFlightStartOrientation[] = "flightStartOrientation";
static constexpr char kFlightStartPosition[] = "flightStartPosition";
static constexpr char kFovDirection[] = "fovDirection";
static constexpr char kFovDegrees[] = "fovDegrees";
static constexpr char kFarPlane[] = "farPlane";
static constexpr char kMode[] = "mode";
static constexpr char kOrbitHomePosition[] = "orbitHomePosition";
static constexpr char kOrbitSpeed[] = "orbitSpeed";
static constexpr char kScaling[] = "scaling";
static constexpr char kShift[] = "shift";
static constexpr char kTargetPosition[] = "targetPosition";
static constexpr char kUpVector[] = "upVector";
static constexpr char kZoomSpeed[] = "zoomSpeed";
static constexpr char kFocalLength[] = "focalLength";
static constexpr char kAspect[] = "aspect";
static constexpr char kNear[] = "near";
static constexpr char kFar[] = "far";
static constexpr char kAperture[] = "aperture";
static constexpr char kSensitivity[] = "sensitivity";
static constexpr char kShutterSpeed[] = "shutterSpeed";
static constexpr char kLeft[] = "left";
static constexpr char kRight[] = "right";
static constexpr char kBottom[] = "bottom";
static constexpr char kTop[] = "top";
static constexpr char kFovInDegrees[] = "fovInDegrees";
static constexpr char kAutoPlay[] = "autoPlay";
static constexpr char kIndex[] = "index";
static constexpr char kAnimation[] = "animation";
static constexpr char kLoop[] = "loop";
static constexpr char kResetToTPoseOnReset[] = "resetToTPoseOnReset";
static constexpr char kPlaybackSpeed[] = "playbackSpeed";
static constexpr char kNotifyOfAnimationEvents[] = "notifyOfAnimationEvents";
static constexpr char kType[] = "type";
static constexpr char kColor[] = "color";
static constexpr char kColorTemperature[] = "colorTemperature";
static constexpr char kIntensity[] = "intensity";
static constexpr char kCastLight[] = "castLight";
static constexpr char kFalloffRadius[] = "falloffRadius";
static constexpr char kSpotLightConeInner[] = "spotLightConeInner";
static constexpr char kSpotLightConeOuter[] = "spotLightConeOuter";
static constexpr char kSunAngularRadius[] = "sunAngularRadius";
static constexpr char kSunHaloSize[] = "sunHaloSize";
static constexpr char kSunHaloFalloff[] = "sunHaloFalloff";

// specific collidable values:
static constexpr char kCollidable[] = "collidable";
static constexpr char kCollidableShapeType[] = "collidable_shapeType";
static constexpr char kCollidableExtents[] = "collidable_extentSize";
static constexpr char kCollidableIsStatic[] = "collidable_isStatic";
static constexpr char kCollidableLayer[] = "collidable_layer";
static constexpr char kCollidableMask[] = "collidable_mask";
static constexpr char kCollidableShouldMatchAttachedObject[] =
  "collidable_shouldMatchAttachedObject";

// Custom model viewer for sending frames to dart.
static constexpr char kUpdateFrame[] = "updateFrame";
static constexpr char kPreRenderFrame[] = "preRenderFrame";
static constexpr char kRenderFrame[] = "renderFrame";
static constexpr char kPostRenderFrame[] = "postRenderFrame";
static constexpr char kParam_TimeSinceLastRenderedSec[] = "timeSinceLastRenderedSec";
static constexpr char kParam_FPS[] = "fps";
static constexpr char kParam_ElapsedFrameTime[] = "elapsedFrameTime";

// Collision Manager and uses, sending messages to dart from native
static constexpr char kCollisionEvent[] = "collision_event";
static constexpr char kCollisionEventSourceGuid[] = "collision_event_source";
static constexpr char kCollisionEventHitCount[] = "collision_event_hit_count";
static constexpr char kCollisionEventHitResult[] = "collision_event_hit_result_";
static constexpr char kCollisionEventType[] = "collision_event_type";
enum CollisionEventType {
  eFromNonNative,
  eNativeOnTouchBegin,
  eNativeOnTouchHeld,
  eNativeOnTouchEnd
};

static constexpr char kAnimationEvent[] = "animation_event";
static constexpr char kAnimationEventType[] = "animation_event_type";
enum AnimationEventType {
  eAnimationStarted,
  eAnimationEnded,
  // might want paused, un-paused, speed change.
};
static constexpr char kAnimationEventData[] = "animation_event_data";

static constexpr char kCamera_Inertia_RotationSpeed[] = "inertia_rotationSpeed";
static constexpr char kCamera_Inertia_VelocityFactor[] = "inertia_velocityFactor";
static constexpr char kCamera_Inertia_DecayFactor[] = "inertia_decayFactor";

static constexpr char kCamera_Pan_angleCapX[] = "pan_angleCapX";
static constexpr char kCamera_Pan_angleCapY[] = "pan_angleCapY";
static constexpr char kCamera_Zoom_minCap[] = "zoom_minCap";
static constexpr char kCamera_Zoom_maxCap[] = "zoom_maxCap";

// Configuration values stored in ecs for easier lookup
static constexpr char kAssetPath[] = "assetPath";
static constexpr char kIsGlb[] = "isGlb";
static constexpr char kModelInstancingMode[] = "instancingMode";

}  // namespace plugin_filament_view
