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

#include <filament-iblprefilter/IBLPrefilterContext.h>
#include <filament/Engine.h>

namespace plugin_filament_view {
/**
 * IBLPrefilter creates and initializes GPU state common to all environment map
 * filters
 *
 * Typically, only one instance per filament Engine of this object needs to
 * exist.
 *
 * @see IBLPrefilterContext
 */
class IBLProfiler {
 public:
  explicit IBLProfiler(::filament::Engine* engine)
      : context_(IBLPrefilterContext(*engine)),
        equirectangularToCubeMap_(
            IBLPrefilterContext::EquirectangularToCubemap(context_)),
        specularFilter_(IBLPrefilterContext::SpecularFilter(context_)) {}

  /**
   * Converts an equirectangular image to a cubemap.
   *
   * @param equirect Texture to convert to a cubemap.
   * - Can't be null.
   * - Must be a 2d texture
   * - Must have equirectangular geometry, that is width == 2*height.
   * - Must be allocated with all mip levels.
   * - Must be SAMPLEABLE
   *
   * @return the cubemap texture
   *
   * @see IBLPrefilterContext.EquirectangularToCubemap
   */
  ::filament::Texture* createCubeMapTexture(
      ::filament::Texture const* equirect) {
    return equirectangularToCubeMap_(equirect);
  }

  /**
   * Generates a prefiltered cubemap.
   *
   * SpecularFilter is a GPU based implementation of the specular probe
   * pre-integration filter.
   *
   * ** Launch the heaver computation. Expect 100-100ms on the GPU.**
   *
   * @param skybox Environment cubemap.
   * This cubemap is SAMPLED and have all its levels allocated.
   *
   * @return the reflections texture
   */
  ::filament::Texture* getLightReflection(::filament::Texture* skybox) {
    return specularFilter_(skybox);
  }

 private:
  IBLPrefilterContext context_;

  /**
   * EquirectangularToCubemap is use to convert an equirectangular image to a
   * cubemap
   *
   * Creates a EquirectangularToCubemap processor.
   */
  IBLPrefilterContext::EquirectangularToCubemap equirectangularToCubeMap_;

  /**
   * Created specular (reflections) filter. This operation generates the kernel,
   * so it's important to keep it around if it will be reused for several
   * cubemaps. An instance of SpecularFilter is needed per filter configuration.
   * A filter configuration contains the filter's kernel and sample count.
   */
  IBLPrefilterContext::SpecularFilter specularFilter_;
};
}  // namespace plugin_filament_view