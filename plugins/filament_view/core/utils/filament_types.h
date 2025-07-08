#include <filament/utils/Entity.h>
#include <filament/utils/EntityInstance.h>

#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>

namespace plugin_filament_view {

/// Represents an entity in the Filament engine.
/// Lightweight object: can be passed around by value (it's just an ID).
using FilamentEntity = utils::Entity;

/// Represents a Filament entity instance.
/// Lightweight object: can be passed around by value (it's just an ID).
template<typename T> using FilamentEntityInstance = utils::EntityInstance<T>;
using FilamentTransformInstance = FilamentEntityInstance<filament::TransformManager>;
using FilamentRenderableInstance = FilamentEntityInstance<filament::RenderableManager>;
using FilamentCameraComponent = filament::Camera;

};  // namespace plugin_filament_view
