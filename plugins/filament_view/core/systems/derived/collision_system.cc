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
#include <core/systems/ecsystems_manager.h>
#include <filament/Scene.h>
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
bool CollisionSystem::bHasEntityObjectRepresentation(
    const EntityGUID guid) const {
  return collidablesDebugDrawingRepresentation_.find(guid) !=
         collidablesDebugDrawingRepresentation_.end();
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vAddCollidable(EntityObject* collidable) {
  if (!collidable->HasComponentByStaticTypeID(Collidable::StaticGetTypeID())) {
    spdlog::error(
        "You tried to add an entityObject that didnt have a collidable on it, "
        "bailing out.");
    return;
  }

  const auto originalCollidable = dynamic_cast<Collidable*>(
      collidable->GetComponentByStaticTypeID(Collidable::StaticGetTypeID())
          .get());

  if (originalCollidable != nullptr &&
      originalCollidable->GetShouldMatchAttachedObject()) {
    // if it's a shape
    if (const auto originalShape = dynamic_cast<shapes::BaseShape*>(collidable);
        originalShape != nullptr) {
      originalCollidable->SetShapeType(originalShape->type_);
      originalCollidable->SetExtentsSize(
          originalShape->m_poBaseTransform.lock()->GetExtentsSize());
    }

    // modeled handled below
  }

  // make the BaseShape Object
  shapes::BaseShape* newShape = nullptr;
  if (dynamic_cast<Model*>(collidable)) {
    const auto ourModelObject = dynamic_cast<Model*>(collidable);
    const auto ourAABB = ourModelObject->poGetBoundingBox();

    newShape = new shapes::Cube();
    newShape->m_bDoubleSided = false;
    newShape->type_ = ShapeType::Cube;

    ourModelObject->vShallowCopyComponentToOther(
        BaseTransform::StaticGetTypeID(), *newShape);
    ourModelObject->vShallowCopyComponentToOther(
        CommonRenderable::StaticGetTypeID(), *newShape);

    const std::shared_ptr<Component> componentBT =
        newShape->GetComponentByStaticTypeID(BaseTransform::StaticGetTypeID());
    const std::shared_ptr<BaseTransform> baseTransformPtr =
        std::dynamic_pointer_cast<BaseTransform>(componentBT);

    const std::shared_ptr<Component> componentCR =
        newShape->GetComponentByStaticTypeID(
            CommonRenderable::StaticGetTypeID());
    const std::shared_ptr<CommonRenderable> commonRenderablePtr =
        std::dynamic_pointer_cast<CommonRenderable>(componentCR);

    newShape->m_poBaseTransform =
        std::weak_ptr<BaseTransform>(baseTransformPtr);
    newShape->m_poCommonRenderable =
        std::weak_ptr<CommonRenderable>(commonRenderablePtr);

    const auto& ourTransform = baseTransformPtr;

    // Note I believe this is correct; more thorough testing is needed; there's
    // a concern around exporting models not centered at 0,0,0 and not being
    // 100% accurate.
    ourTransform->SetPosition(ourAABB.center() +
                                    ourTransform->GetPosition());
    ourTransform->SetExtentsSize(ourAABB.extent());
    ourTransform->SetScale(ourAABB.extent());

    if (originalCollidable != nullptr &&
        originalCollidable->GetShouldMatchAttachedObject()) {
      originalCollidable->SetCenterPoint(ourTransform->GetPosition());

      originalCollidable->SetShapeType(ShapeType::Cube);
      originalCollidable->SetExtentsSize(ourAABB.extent());
    }
  } else if (dynamic_cast<shapes::Cube*>(collidable)) {
    const auto originalObject = dynamic_cast<shapes::Cube*>(collidable);
    newShape = new shapes::Cube();
    originalObject->CloneToOther(*newShape);
  } else if (dynamic_cast<shapes::Sphere*>(collidable)) {
    const auto originalObject = dynamic_cast<shapes::Sphere*>(collidable);
    newShape = new shapes::Sphere();
    originalObject->CloneToOther(*newShape);
  } else if (dynamic_cast<shapes::Plane*>(collidable)) {
    const auto originalObject = dynamic_cast<shapes::Plane*>(collidable);
    newShape = new shapes::Plane();
    originalObject->CloneToOther(*newShape);
  }

  if (newShape == nullptr) {
    // log not handled;
    spdlog::error("Failed to create collidable shape.");
    return;
  }

  collidables_.push_back(collidable);

  newShape->m_bIsWireframe = true;

  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "vAddCollidable");
  const auto engine = filamentSystem->getFilamentEngine();

  filament::Scene* poFilamentScene = filamentSystem->getFilamentScene();
  utils::EntityManager& oEntityManager = engine->getEntityManager();

  const auto oEntity = std::make_shared<Entity>(oEntityManager.create());

  newShape->bInitAndCreateShape(engine, oEntity);
  poFilamentScene->addEntity(*oEntity);

  // now store in map.
  collidablesDebugDrawingRepresentation_.insert(
      std::pair(collidable->GetGuid(), newShape));
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vRemoveCollidable(EntityObject* collidable) {
  collidables_.remove(collidable);

  // Remove from collidablesDebugDrawingRepresentation_
  const auto iter =
      collidablesDebugDrawingRepresentation_.find(collidable->GetGuid());
  if (iter != collidablesDebugDrawingRepresentation_.end()) {
    delete iter->second;
    collidablesDebugDrawingRepresentation_.erase(iter);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vTurnOnRenderingOfCollidables() const {
  for (const auto& [fst, snd] : collidablesDebugDrawingRepresentation_) {
    snd->vRemoveEntityFromScene();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vTurnOffRenderingOfCollidables() const {
  for (const auto& [fst, snd] : collidablesDebugDrawingRepresentation_) {
    snd->vAddEntityToScene();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::DebugPrint() {
  spdlog::debug("{}", __FUNCTION__);

  /*for (auto& collidable : collidables_) {
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

  // Iterate over all entities.
  for (const auto& entity : collidables_) {
    // Make sure collidable is still here....
    auto collidable = std::dynamic_pointer_cast<Collidable>(
        entity->GetComponentByStaticTypeID(Collidable::StaticGetTypeID()));
    if (!collidable) {
      continue;  // No collidable component, skip this entity
    }

    // Check if the collision layer matches (if a specific layer was provided)
    // if (collisionLayer != 0 && (collidable->GetCollisionLayer() &
    // collisionLayer) == 0) {
    //    continue; // Skip if layers don't match
    // }

    // Perform intersection test with the ray
    if (filament::math::float3 hitLocation;
        collidable->bDoesIntersect(rayCast, hitLocation)) {
      // If there is an intersection, create a HitResult
      HitResult hitResult;
      hitResult.guid_ = entity->GetGuid();
      hitResult.name_ = entity->GetName();
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
void CollisionSystem::vInitSystem() {
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

        for (const auto& entity : collidables_) {
          if (entity->GetGuid() == guid) {
            const auto collidable = std::dynamic_pointer_cast<Collidable>(
                entity->GetComponentByStaticTypeID(
                    Collidable::StaticGetTypeID()));

            collidable->SetEnabled(value);

            break;
          }
        }
      });
}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vUpdate(float /*fElapsedTime*/) {}

/////////////////////////////////////////////////////////////////////////////////////////
void CollisionSystem::vShutdownSystem() {}

}  // namespace plugin_filament_view
