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

#include <core/components/base/component.h>
#include <core/utils/filament_types.h>

namespace plugin_filament_view {

using FilamentRenderableInstance = utils::EntityInstance<filament::RenderableManager>;

class CommonRenderable : public Component {
 public:
  // Constructor
  CommonRenderable()
      : Component(std::string(__FUNCTION__)),
        m_bCullingOfObjectEnabled(true),
        m_bReceiveShadows(true),
        m_bCastShadows(true) {}
  explicit CommonRenderable(const flutter::EncodableMap& params);

  FilamentRenderableInstance _fInstance;

  // Getters
  [[nodiscard]] bool IsCullingOfObjectEnabled() const {
    return m_bCullingOfObjectEnabled;
  }

  [[nodiscard]] bool IsReceiveShadowsEnabled() const {
    return m_bReceiveShadows;
  }

  [[nodiscard]] bool IsCastShadowsEnabled() const { return m_bCastShadows; }

  // Setters
  void SetCullingOfObjectEnabled(bool enabled) {
    m_bCullingOfObjectEnabled = enabled;
  }

  void SetReceiveShadows(bool enabled) { m_bReceiveShadows = enabled; }

  void SetCastShadows(bool enabled) { m_bCastShadows = enabled; }

  void DebugPrint(const std::string& tabPrefix) const override;

  [[nodiscard]] Component* Clone() const override {
    /// TODO: fix this
    // return new CommonRenderable(*this);  // Copy constructor is called here
    return nullptr;
  }

 private:
  bool m_bCullingOfObjectEnabled;
  bool m_bReceiveShadows;
  bool m_bCastShadows;
};

}  // namespace plugin_filament_view