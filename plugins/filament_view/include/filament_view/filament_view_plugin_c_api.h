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

#ifndef FLUTTER_PLUGIN_FILAMENT_VIEW_PLUGIN_C_API_H_
#define FLUTTER_PLUGIN_FILAMENT_VIEW_PLUGIN_C_API_H_

#include "platform_view_listener.h"
#include <flutter_homescreen.h>
#include <flutter_plugin_registrar.h>

#include <string>
#include <vector>

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define FLUTTER_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#if defined(__cplusplus)
extern "C" {
#endif

FLUTTER_PLUGIN_EXPORT void FilamentViewPluginCApiRegisterWithRegistrar(
  FlutterDesktopPluginRegistrar* registrar,
  int32_t id,
  const std::string& viewType,
  int32_t direction,
  double top,
  double left,
  double width,
  double height,
  const std::vector<uint8_t>& params,
  const std::string& assetDirectory,
  FlutterDesktopEngineRef engine,
  PlatformViewAddListener addListener,
  PlatformViewRemoveListener removeListener,
  void* platform_view_context
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // FLUTTER_PLUGIN_FILAMENT_VIEW_PLUGIN_C_API_H_
