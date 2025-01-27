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

#include "include/rive_text/rive_text_plugin_c_api.h"

#include "flutter/plugin_registrar.h"

#include "common/common.h"
#include "librive_text.h"
#include "rive_text_plugin.h"

void RiveTextPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  if (!plugin_rive_text::LibRiveText::IsPresent()) {
    plugin_rive_text::RiveTextPlugin::RegisterWithRegistrar(
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrar>(registrar));
  } else {
    spdlog::debug("librive_text.sp not found. Rive plugin will not be loaded.");
  }
}
