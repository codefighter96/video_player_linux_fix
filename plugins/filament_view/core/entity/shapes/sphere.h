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

#include "core/entity/shapes/baseshape.h"

namespace plugin_filament_view {

using ::utils::Entity;

namespace shapes {

class Sphere : public BaseShape {
 public:
  Sphere(const std::string& flutter_assets_path,
         const flutter::EncodableMap& params);
  Sphere();
  ~Sphere() override = default;

  // Disallow copy and assign.
  Sphere(const Sphere&) = delete;
  Sphere& operator=(const Sphere&) = delete;

  void DebugPrint(const char* tag) const override;

  bool bInitAndCreateShape(::filament::Engine* engine_,
                           std::shared_ptr<Entity> entityObject) override;
  void CloneToOther(BaseShape& other) const override;

 private:
  static void createDoubleSidedSphere(::filament::Engine* engine_);

  void createSingleSidedSphere(::filament::Engine* engine_);

  int stacks_;
  int slices_;

  std::vector<::filament::math::float3> vertices_;
  std::vector<::filament::math::float3> normals_;
  std::vector<unsigned short> indices_;
  std::vector<::filament::math::float2> uvs_;
};

}  // namespace shapes
}  // namespace plugin_filament_view
