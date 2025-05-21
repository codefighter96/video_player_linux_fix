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

#include <filament/math/quat.h>
#include <filament/math/mat4.h>

#include <core/entity/base/entityobject.h>
#include <core/components/base/component.h>
#include <core/utils/filament_types.h>
#include <core/utils/asserts.h>

namespace plugin_filament_view {

/// @brief TransformVectorData is a struct that holds position/scale/rotation data.
struct TransformVectorData {
  union {
    filament::math::float3 position;
    float position_data[3];
    struct {
      float posX = 0;
      float posY = 0;
      float posZ = 0;
    };
  };

  union {
    filament::math::float3 scale;
    float scale_data[3];
    struct {
      float scaleX = 1;
      float scaleY = 1;
      float scaleZ = 1;
    };
  };

  union {
    filament::math::quatf rotation;
    float rotation_data[4];
    struct {
      float rotX = 0;
      float rotY = 0;
      float rotZ = 0;
      float rotW = 1;
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
    /// @brief _isDirty is a flag that indicates if the transform has been modified.
    /// If true, the transform will be updated by the [TransformSystem] on this frame
    /// and the flag will be set to false.
    /// It's set to true whenever the transform is modified.
    bool _isDirty = true;
    EntityGUID _parentId = kNullGuid;
    bool _isParentDirty = false;

  public:
    TransformVectorData local;
    /// @brief Computed transform matrix in world space.
    /// TODO: does this have to be stored as value? Evaluate storing pointer only
    TransformMatrixData global;
    FilamentTransformInstance _fInstance;

    BaseTransform()
          : Component(std::string(__FUNCTION__)),
            local({{kFloat3Zero}, {kFloat3One}, {kQuatfIdentity}}),
            global({kMat4fIdentity}) {}

    explicit BaseTransform(const filament::math::float3& position,
                           const filament::math::float3& scale,
                           const filament::math::quatf& rotation)
          : Component(std::string(__FUNCTION__)),
            local({{position}, {scale}, {rotation}}),
            global({kMat4fIdentity}) {}

    explicit BaseTransform(const TransformVectorData& localTransform)
          : Component(std::string(__FUNCTION__)),
            local(localTransform),
            global({kMat4fIdentity}) {}

    // explicit BaseTransform(const filament::math::float3& position,
    //                        const filament::math::float3& scale)
    //       : Component(std::string(__FUNCTION__)),
    //         local({position, scale, kQuatfIdentity}),
    //         global({kMat4fIdentity}) {}

    // explicit BaseTransform(const filament::math::float3& position)
    //       : Component(std::string(__FUNCTION__)),
    //         local({position, kFloat3One, kQuatfIdentity}),
    //         global({kMat4fIdentity}) {}
            
    explicit BaseTransform(const flutter::EncodableMap& params);

    /*
     *   Local
     */
    // Getters
    [[nodiscard]] inline EntityGUID GetParentId() const {
      return _parentId;
    }

    [[nodiscard]] inline const filament::math::float3& GetPosition() const {
      return local.position;
    }

    [[nodiscard]] inline const filament::math::float3& GetScale() const {
      return local.scale;
    }

    [[nodiscard]] inline const filament::math::quatf& GetRotation() const {
      return local.rotation;
    }

    // Setters
    inline void SetPosition(const filament::math::float3& position) {
      local.position = position;
      _isDirty = true;
    }

    inline void SetScale(const filament::math::float3& scale) {
      // assert positive scale
      /// TODO: if interested in supporting e.g. negative scalings and shear 
      ///       should look at Graphics Gems II §VII.1.
      runtime_assert(
        scale.x > 0 && scale.y > 0 && scale.z > 0,
        "Scale must be positive"
      );

      local.scale = scale;
      _isDirty = true;
    }

    inline void SetRotation(const filament::math::quatf& rotation) {
      local.rotation = rotation;
      _isDirty = true;
    }

    /// Sets all transform values at once. All params are optional (nullptr)
    inline void SetTransform(const filament::math::float3* position = nullptr,
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

    inline void SetTransform(
      const filament::math::float3& position,
      const filament::math::float3& scale,
      const filament::math::quatf& rotation
    ) {
      local.position = position;
      local.scale = scale;
      local.rotation = rotation;
      _isDirty = true;
    }

    void SetTransform(const filament::math::mat4f& localMatrix);

    inline void setParent(EntityGUID parentId) {
      _parentId = parentId;
      _isParentDirty = true;
    }

    inline void SetDirty(bool dirty) {
      _isDirty = dirty;
    }

    inline void SetParentDirty(bool dirty) {
      _isParentDirty = dirty;
    }



    /*
     *   Global
     */
    // Getters
    [[nodiscard]] inline bool IsDirty() const {
      return _isDirty;
    }

    [[nodiscard]] inline bool IsParentDirty() const {
      return _isParentDirty;
    }

    /// @returns The transform matrix in world space.    
    [[nodiscard]] inline const filament::math::mat4f& GetGlobalMatrix() const {
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
    
    [[nodiscard]] Component* Clone() const override {
      return new BaseTransform(*this);
    }
};

}  // namespace plugin_filament_view