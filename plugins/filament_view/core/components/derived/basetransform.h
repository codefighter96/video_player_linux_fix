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
#include <filament/math/quat.h>
#include <filament/math/mat4.h>

namespace plugin_filament_view {

/// @brief TransformVectorData is a struct that holds position/scale/rotation data.
struct TransformVectorData {
  union {
    filament::math::float3 position;
    float position_data[3];
    struct {
      float posX;
      float posY;
      float posZ;
    };
  };

  union {
    filament::math::float3 scale;
    float scale_data[3];
    struct {
      float scaleX;
      float scaleY;
      float scaleZ;
    };
  };

  union {
    filament::math::quatf rotation;
    float rotation_data[4];
    struct {
      float rotX;
      float rotY;
      float rotZ;
      float rotW;
    };
  };
};

/// @brief TransformMatrixData is a struct that holds a computed transform matrix's data.
struct TransformMatrixData {
  union {
    filament::math::mat4f matrix;
    float matrix_data[16];
  };
};

static constexpr filament::math::float3 kFloat3Zero = {0, 0, 0};
static constexpr filament::math::float3 kFloat3One = {1, 1, 1};
static constexpr filament::math::quatf kQuatfIdentity = {0, 0, 0, 1};
static constexpr filament::math::mat4f kMat4fIdentity = filament::math::mat4f();

class BaseTransform : public Component {
  private:
    bool _isDirty = false;
    filament::math::float3 _extentSize;

  public:
    TransformVectorData local;
    TransformMatrixData global;

    BaseTransform()
          : Component(std::string(__FUNCTION__)),
            _extentSize(kFloat3One),
            local({{kFloat3Zero}, {kFloat3One}, {kQuatfIdentity}}),
            global({kMat4fIdentity}) {}


    explicit BaseTransform(const flutter::EncodableMap& params);

    /*
     *   Local
     */
    // Getters
    [[nodiscard]] const filament::math::float3& GetPosition() const {
      return local.position;
    }

    [[nodiscard]] const filament::math::float3& GetExtentsSize() const {
      return _extentSize;
    }

    [[nodiscard]] const filament::math::float3& GetScale() const {
      return local.scale;
    }

    [[nodiscard]] const filament::math::quatf& GetRotation() const {
      return local.rotation;
    }

    // Setters
    void SetPosition(const filament::math::float3& position) {
      local.position = position;
      _isDirty = true;
    }

    void SetExtentsSize(const filament::math::float3& extentsSize) {
      _extentSize = extentsSize;
      // NOTE: doesn't set dirty to true as it doesn't affect the transform matrix
    }

    void SetScale(const filament::math::float3& scale) {
      local.scale = scale;
      _isDirty = true;
    }

    void SetRotation(const filament::math::quatf& rotation) {
      local.rotation = rotation;
      _isDirty = true;
    }

    /// Sets all transform values at once. All params are optional (nullptr)
    void SetTransform(const filament::math::float3* position = nullptr,
                      const filament::math::float3* scale = nullptr,
                      const filament::math::quatf* rotation = nullptr) {
      if (position) {
        local.position = *position;
      }

      if (scale) {
        local.scale = *scale;
      }
      
      if (rotation) {
        local.rotation = *rotation;
      }

      _isDirty = true;
    }



    /*
     *   Global
     */
    // Getters
    [[nodiscard]] bool IsDirty() const {
      return _isDirty;
    }

    [[nodiscard]] const filament::math::mat4f& GetGlobalMatrix() const {
      return global.matrix;
    }

    // TODO
    // [[nodiscard]] const filament::math::float3& GetGlobalPosition() const {
      
    // }

    // TODO
    // [[nodiscard]] const filament::math::float3& GetGlobalScale() const {

    // }

    // TODO
    // [[nodiscard]] const filament::math::quatf& GetGlobalRotation() const {

    // }

    // Setters

    // TODO
    // void SetGlobalMatrix(const filament::math::mat4f& matrix) {
    //   throw std::runtime_error("Not implemented");
    // }

    // TODO
    // void SetGlobalPosition(const filament::math::float3& position) {
    
    // }

    // TODO
    // void SetGlobalScale(const filament::math::float3& scale) {

    // }

    // TODO
    // void SetGlobalRotation(const filament::math::quatf& rotation) {

    // }

    /*
     *   Utils
     */
    void DebugPrint(const std::string& tabPrefix) const override;
    
    static size_t StaticGetTypeID() { return typeid(BaseTransform).hash_code(); }

    [[nodiscard]] size_t GetTypeID() const override { return StaticGetTypeID(); }

    [[nodiscard]] Component* Clone() const override {
      return new BaseTransform(*this);
    }
};

}  // namespace plugin_filament_view