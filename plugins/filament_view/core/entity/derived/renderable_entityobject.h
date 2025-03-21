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

#include <encodable_value.h>

#include <core/entity/base/entityobject.h>

namespace plugin_filament_view {

// Renderable Entity Objects are intended to have material settings on them
// where NonRenderable EntityObjects do not. Its expected on play Renderable's
// have the ability to show up in the scene as models/shapes/objects
// where NonRenderables are more data without a physical representation
// NonRenderables are great for items like 'Global Light', Camera, hidden
// collision
class RenderableEntityObject : public EntityObject {
  friend class MaterialSystem;

 public:
 protected:
  RenderableEntityObject() : EntityObject() {}
  explicit RenderableEntityObject(const flutter::EncodableMap& params)
      : EntityObject(params) {}
  explicit RenderableEntityObject(const std::string& name,
                                  const EntityGUID guid)
      : EntityObject(name, guid) {}
  virtual void DebugPrint() const = 0;

  // These are expected to have Material instances in base class after we go
  // from Uber shader to <?more interchangeable?> on models. For now these are
  // not implemented on Models, but are on BaseShapes.

  // This is a heavy lift function as it will recreate / load a material
  // if it doesn't exist and reset everything from scratch.
  virtual void vChangeMaterialDefinitions(const flutter::EncodableMap& params,
                                          const TextureMap& loadedTextures) = 0;
  virtual void vChangeMaterialInstanceProperty(
      const MaterialParameter* materialParam,
      const TextureMap& loadedTextures) = 0;

 private:
};
}  // namespace plugin_filament_view