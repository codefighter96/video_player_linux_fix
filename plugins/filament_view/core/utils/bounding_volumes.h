
#pragma once

#include <filament/Box.h>

namespace plugin_filament_view {

using AABB = filament::Box;

class BoundingSphere {
 public:
  filament::math::float3 center;
  float radius;

  BoundingSphere() : center(filament::math::float3(0, 0, 0)), radius(0) {}

  explicit BoundingSphere(const filament::math::float3& center, float radius)
      : center(center), radius(radius) {}

  explicit BoundingSphere(const AABB& aabb) : center(aabb.center), radius(0) {
    // Set radius to max half extent
    const filament::math::float3 halfExtent = aabb.halfExtent;
    radius = std::max({halfExtent.x, halfExtent.y, halfExtent.z});
  }
};

}  // namespace plugin_filament_view