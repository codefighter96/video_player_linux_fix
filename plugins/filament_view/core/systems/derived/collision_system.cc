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
#include "collision_system.h"

#include "filament_system.h"

#include <core/entity/derived/model/model.h>
#include <core/entity/derived/shapes/cube.h>
#include <core/entity/derived/shapes/plane.h>
#include <core/entity/derived/shapes/sphere.h>
#include <core/systems/derived/shape_system.h>
#include <core/utils/entitytransforms.h>
#include <core/utils/asserts.h>
#include <core/systems/ecs.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

/////////////////////////////////////////////////////////////////////////////////////////
flutter::EncodableValue HitResult::Encode() const {
  // Convert float3 to a list of floats
  flutter::EncodableList hitPosition = {
      flutter::EncodableValue(hitPosition_.x),
      flutter::EncodableValue(hitPosition_.y),
      flutter::EncodableValue(hitPosition_.z)};

  // Create a map to represent the HitResult
  flutter::EncodableMap encodableMap = {
      {flutter::EncodableValue("guid"), flutter::EncodableValue(guid_)},
      {flutter::EncodableValue("name"), flutter::EncodableValue(name_)},
      {flutter::EncodableValue("hitPosition"),
       flutter::EncodableValue(hitPosition)}};

  return flutter::EncodableValue(encodableMap);
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vTurnOnRenderingOfCollidables() const {
  const auto colliders = ecs->getComponentsOfType<Collidable>();
  for(const auto& collider : colliders) {
    const auto wireframe = collider->_wireframe;
    if(!!wireframe) {
      wireframe->vAddEntityToScene();
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vTurnOffRenderingOfCollidables() const {
  const auto colliders = ecs->getComponentsOfType<Collidable>();
  for(const auto& collider : colliders) {
    const auto wireframe = collider->_wireframe;
    if(!!wireframe) {
      wireframe->vRemoveEntityFromScene();
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::DebugPrint() {
  spdlog::debug("{}", __FUNCTION__);

  /*for (auto& collidable : collidables) {
    collidable->DebugPrint();
  }*/
}

/////////////////////////////////////////////////////////////////////////////////////////
inline float fLength2(const filament::math::float3& v) {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

/////////////////////////////////////////////////////////////////////////////////////////
std::list<HitResult> CollisionSystem::lstCheckForCollidable(
    Ray& rayCast,
    int64_t /*collisionLayer*/) const {
  std::list<HitResult> hitResults;

  const auto collidables = ecs->getEntitiesWithComponent<Collidable>();

  // Iterate over all entities.
  for (const auto& entity : collidables) {
    const EntityGUID guid = entity->GetGuid();
    auto collidable = ecs->getComponent<Collidable>(guid);
    debug_assert(!!collidable, fmt::format("Collidable missing for entity: {}", guid));
    if (!collidable->enabled) continue;

    // Check if the collision layer matches (if a specific layer was provided)
    // if (collisionLayer != 0 && (collidable->GetCollisionLayer() &
    // collisionLayer) == 0) {
    //    continue; // Skip if layers don't match
    // }

    const auto transform = ecs->getComponent<BaseTransform>(guid);

    // Perform intersection test with the ray
    if (
      filament::math::float3 hitLocation;
      collidable->intersects(rayCast, hitLocation, transform)
    ) {
      // If there is an intersection, create a HitResult
      HitResult hitResult;
      hitResult.guid_ = guid;
      hitResult.name_ = collidable->eventName;
      hitResult.hitPosition_ = hitLocation;  // Set the hit location

      SPDLOG_INFO("HIT RESULT: {}", hitResult.guid_);

      // Add to the hit results
      hitResults.push_back(hitResult);
    }
  }

  // Sort hit results by distance from the ray's origin
  hitResults.sort([&rayCast](const HitResult& a, const HitResult& b) {
    // Calculate the squared distance to avoid the cost of sqrt
    const auto distanceA = fLength2(a.hitPosition_ - rayCast.f3GetPosition());
    const auto distanceB = fLength2(b.hitPosition_ - rayCast.f3GetPosition());

    // Sort in ascending order (closest hit first)
    return distanceA < distanceB;
  });

  // Return the sorted list of hit results
  return hitResults;
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::SendCollisionInformationCallback(
    const std::list<HitResult>& lstHitResults,
    std::string sourceQuery,
    const CollisionEventType eType) const {
  flutter::EncodableMap encodableMap;

  // event type
  encodableMap[flutter::EncodableValue(kCollisionEventType)] =
      static_cast<int>(eType);
  // source guid
  encodableMap[flutter::EncodableValue(kCollisionEventSourceGuid)] =
      sourceQuery;
  // hit count
  encodableMap[flutter::EncodableValue(kCollisionEventHitCount)] =
      static_cast<int>(lstHitResults.size());

  int iter = 0;
  for (const auto& arg : lstHitResults) {
    std::ostringstream oss;
    oss << kCollisionEventHitResult << iter;

    encodableMap[flutter::EncodableValue(oss.str())] = arg.Encode();

    ++iter;
  }

  vSendDataToEventChannel(encodableMap);
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vOnInitSystem() {
  vRegisterMessageHandler(
      ECSMessageType::CollisionRequest, [this](const ECSMessage& msg) {
        auto rayInfo = msg.getData<Ray>(ECSMessageType::CollisionRequest);
        const auto requestor =
            msg.getData<std::string>(ECSMessageType::CollisionRequestRequestor);
        const auto type = msg.getData<CollisionEventType>(
            ECSMessageType::CollisionRequestType);

        const auto hitList = lstCheckForCollidable(rayInfo, 0);

        SendCollisionInformationCallback(hitList, requestor, type);
      });

  vRegisterMessageHandler(
      ECSMessageType::ToggleDebugCollidableViewsInScene,
      [this](const ECSMessage& msg) {
        spdlog::debug("ToggleDebugCollidableViewsInScene");

        const auto value = msg.getData<bool>(
            ECSMessageType::ToggleDebugCollidableViewsInScene);

        if (!value) {
          vTurnOffRenderingOfCollidables();
        } else {
          vTurnOnRenderingOfCollidables();
        }

        spdlog::debug("ToggleDebugCollidableViewsInScene Complete");
      });

  vRegisterMessageHandler(
      ECSMessageType::ToggleCollisionForEntity, [this](const ECSMessage& msg) {
        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ToggleCollisionForEntity);
        const auto value = msg.getData<bool>(ECSMessageType::BoolValue);

        if (const auto collidable = ecs->getComponent<Collidable>(guid); !!collidable) {
          collidable->enabled = value;
        }
      });
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vUpdate(float /*fElapsedTime*/) {
  // Iterate over all collidables
  const auto colliders = ecs->getComponentsOfType<Collidable>();
  for (const auto& collidable : colliders) {
    // Check if the collidable is enabled
    if (collidable->enabled) {
      // get pointer to collidable->_aabb
      AABB& aabb = collidable->_aabb;

      // Make sure it has an AABB
      if(aabb.isEmpty()) {
        const auto entity = collidable->entityOwner_;
        spdlog::debug("Collidable entity({}) has no AABB", entity->GetGuid());
        // Get AABB if it's a RenderableEntityObject
        if (const auto renderableEntity = dynamic_cast<RenderableEntityObject*>(entity)) {
          aabb = renderableEntity->getAABB();
          spdlog::debug("  Adding AABB to collidable entity({})", entity->GetGuid());
          spdlog::debug("  AABB.pos: x={}, y={}, z={}", aabb.center.x, aabb.center.y, aabb.center.z);
          spdlog::debug("  AABB.size: x={}, y={}, z={}", aabb.halfExtent.x * 2, aabb.halfExtent.y * 2, aabb.halfExtent.z * 2);
          renderableEntity->getComponent<BaseTransform>()->DebugPrint("  ");
        } else {
          spdlog::error("  Collidable does not have an AABB");
          continue;
        }
      }

      // Make sure it has a wireframe
      if(!collidable->_wireframe) {
        const auto entity = collidable->entityOwner_;
        // Create a cube wireframe
        auto cubeChild = std::make_shared<shapes::Cube>("(collider wireframe)");
        cubeChild->m_bIsWireframe = true;
        cubeChild->addComponent<MaterialDefinitions>(kDefaultMaterial);
        const auto shapeSystem = ecs->getSystem<ShapeSystem>("CollisionSystem::vUpdate");
        ecs->addEntity(cubeChild);
        shapeSystem->addShapeToScene(cubeChild);
        auto childTransform = cubeChild->getComponent<BaseTransform>();
        childTransform->SetTransform(
          aabb.center,
          aabb.halfExtent * 2,
          kQuatfIdentity
        );
        EntityTransforms::vApplyTransform(
          cubeChild->_fEntity,
          kQuatfIdentity,
          aabb.halfExtent * 2,
          aabb.center,
          &(entity->_fEntity)
        );

        collidable->_wireframe = cubeChild;
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vShutdownSystem() {}

}  // namespace plugin_filament_view
