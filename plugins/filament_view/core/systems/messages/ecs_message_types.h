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

enum class ECSMessageType {
  DebugLine,

  CollisionRequest,
  CollisionRequestRequestor,
  CollisionRequestType,

  ViewTargetCreateRequest,
  ViewTargetCreateRequestTop,
  ViewTargetCreateRequestLeft,
  ViewTargetCreateRequestWidth,
  ViewTargetCreateRequestHeight,

  ViewTargetStartRenderingLoops,

  SetCameraFromDeserializedLoad,

  ToggleShapesInScene,
  SetShapeTransform,
  ToggleDebugCollidableViewsInScene,

  ChangeSceneLightProperties,
  ChangeSceneLightPropertiesColorValue,
  ChangeSceneLightPropertiesIntensity,
  ChangeSceneLightTransform,
  Position,
  Rotation,
  Direction,
  Scale,

  ChangeSceneIndirectLightProperties,
  ChangeSceneIndirectLightPropertiesIntensity,

  ChangeViewQualitySettings,
  ChangeViewQualitySettingsWhichView,

  SetFogOptions,

  ChangeMaterialParameter,
  EntityToTarget,
  ChangeMaterialDefinitions,

  ResizeWindow,
  ResizeWindowWidth,
  ResizeWindowHeight,

  MoveWindow,
  MoveWindowTop,
  MoveWindowLeft,

  AnimationEnqueue,
  AnimationClearQueue,
  AnimationPlay,
  AnimationChangeSpeed,
  AnimationPause,
  AnimationResume,
  AnimationSetLooping,

  ChangeTranslationByGUID,
  ChangeRotationByGUID,
  ChangeScaleByGUID,
  floatVec3,
  floatVec4,

  ChangeCameraOrbitHomePosition,
  ChangeCameraTargetPosition,
  ChangeCameraFlightStartPosition,

  ToggleVisualForEntity,
  ToggleCollisionForEntity,
  BoolValue,
};

}
