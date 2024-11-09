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

#include "libnav_render.h"

#include <plugins/common/common.h>

#include <dlfcn.h>

namespace nav_render_view_plugin {

constexpr char kNaviRenderSoName[] = "libnav_render.so";

LibNavRenderExports::LibNavRenderExports(void* lib) {
  if (lib != nullptr) {
    PluginGetFuncAddress(lib, "nav_render_version",
                         &TextureGetInterfaceVersion);
    PluginGetFuncAddress(lib, "nav_render_initialize", &TextureInitialize);
    PluginGetFuncAddress(lib, "nav_render_initialize2", &TextureInitialize2);
    PluginGetFuncAddress(lib, "nav_render_de_initialize", &TextureDeInitialize);
    PluginGetFuncAddress(lib, "nav_render_run_task", &TextureRunTask);
    PluginGetFuncAddress(lib, "nav_render_render", &TextureRender);
    PluginGetFuncAddress(lib, "nav_render_render2", &TextureRender2);
    PluginGetFuncAddress(lib, "nav_render_resize", &TextureResize);

    PluginGetFuncAddress(lib, "comp_surf_version", &SurfaceGetInterfaceVersion);
    PluginGetFuncAddress(lib, "comp_surf_initialize", &SurfaceInitialize);
    PluginGetFuncAddress(lib, "comp_surf_de_initialize", &SurfaceDeInitialize);
    PluginGetFuncAddress(lib, "comp_surf_run_task", &SurfaceRunTask);
    PluginGetFuncAddress(lib, "comp_surf_draw_frame", &SurfaceDrawFrame);
    PluginGetFuncAddress(lib, "comp_surf_resize", &SurfaceResize);
  }
}

LibNavRenderExports* LibNavRender::operator->() const {
  return loadExports();
}

LibNavRenderExports* LibNavRender::loadExports() {
  static LibNavRenderExports exports = [] {
    void* lib;

    if (PluginGetProcAddress(
            RTLD_DEFAULT,
            "comp_surf_initialize"))  // Search the global scope
                                      // for pre-loaded library.
    {
      lib = RTLD_DEFAULT;
    } else {
      lib = dlopen(kNaviRenderSoName, RTLD_LAZY | RTLD_LOCAL);
    }

    return LibNavRenderExports(lib);
  }();

  return exports.SurfaceInitialize ? &exports : nullptr;
}

class LibNavRender LibNavRender;

}  // namespace nav_render_view_plugin
