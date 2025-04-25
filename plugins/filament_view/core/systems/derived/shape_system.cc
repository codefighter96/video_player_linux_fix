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

#include "shape_system.h"

#include <core/components/derived/collidable.h>

#include "filament_system.h"

#include <core/entity/derived/shapes/baseshape.h>
#include <core/entity/derived/shapes/cube.h>
#include <core/entity/derived/shapes/plane.h>
#include <core/entity/derived/shapes/sphere.h>
#include <core/systems/ecs.h>
#include <core/utils/entitytransforms.h>
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <plugins/common/common.h>

#include "collision_system.h"

namespace plugin_filament_view {

using shapes::BaseShape;
using utils::Entity;

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vToggleAllShapesInScene(const bool bValue) const {
  if (bValue) {
    for (const auto& [fst, snd] : m_mapszoShapes) {
      snd->vAddEntityToScene();
    }
  } else {
    for (const auto& [fst, snd] : m_mapszoShapes) {
      snd->vRemoveEntityFromScene();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vToggleSingleShapeInScene(const EntityGUID guid,
                                            const bool bValue) const {
  const auto iter = m_mapszoShapes.find(guid);
  if (iter == m_mapszoShapes.end()) {
    return;
  }

  if (bValue) {
    iter->second->vAddEntityToScene();
  } else {
    iter->second->vRemoveEntityFromScene();
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vRemoveAllShapesInScene() {
  vToggleAllShapesInScene(false);

  for (const auto& [fst, snd] : m_mapszoShapes) {
    ecs->removeEntity(snd->GetGuid());
  }

  m_mapszoShapes.clear();
}

////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<BaseShape> ShapeSystem::poDeserializeShapeFromData(
    const flutter::EncodableMap& mapData) {
  ShapeType type;

  // Find the "shapeType" key in the mapData
  if (const auto it = mapData.find(flutter::EncodableValue("shapeType"));
      it != mapData.end() && std::holds_alternative<int32_t>(it->second)) {
    // Check if the value is within the valid range of the ShapeType enum
    if (int32_t typeValue = std::get<int32_t>(it->second);
        typeValue > static_cast<int32_t>(ShapeType::Unset) &&
        typeValue < static_cast<int32_t>(ShapeType::Max)) {
      type = static_cast<ShapeType>(typeValue);
    } else {
      spdlog::error("Invalid shape type value: {}", typeValue);
      return nullptr;
    }
  } else {
    spdlog::error("shapeType not found or is of incorrect type");
    return nullptr;
  }

  // Based on the type_, create the corresponding shape
  switch (type) {
    case ShapeType::Plane:
      return std::make_unique<shapes::Plane>(mapData);
    case ShapeType::Cube:
      return std::make_unique<shapes::Cube>(mapData);
    case ShapeType::Sphere:
      return std::make_unique<shapes::Sphere>(mapData);
    default:
      // Handle unknown shape type
      spdlog::error("Unknown shape type: {}", static_cast<int32_t>(type));
      return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vRemoveAndReaddShapeToCollisionSystem(
    const EntityGUID guid,
    const std::shared_ptr<BaseShape>& shape) {
  const auto collisionSystem =
      ecs->getSystem<CollisionSystem>(
          "vRemoveAndReaddShapeToCollisionSystem");
  if (collisionSystem == nullptr) {
    spdlog::warn(
        "Failed to get collision system when "
        "vRemoveAndReaddShapeToCollisionSystem");
    return;
  }

  // if we are marked for collidable, have one in the scene, remove and readd
  // if this is a performance issue, we can do the transform move in the future
  // instead.
  if (shape->HasComponent(Component::StaticGetTypeID<Collidable>()) &&
      collisionSystem->hasEntity(guid)) {
    collisionSystem->vRemoveCollidable(shape.get());
    collisionSystem->vAddCollidable(shape.get());
  }
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::addShapesToScene(
    std::vector<std::shared_ptr<BaseShape>>* shapes) {
  SPDLOG_TRACE("++{}", __FUNCTION__);

  // TODO remove this, just debug info print for now;
  /*for (auto& shape : *shapes) {
    shape->DebugPrint("Add shapes to scene");
  }*/

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "addShapesToScene");
  const auto engine = filamentSystem->getFilamentEngine();

  filament::Engine* poFilamentEngine = engine;
  filament::Scene* poFilamentScene = filamentSystem->getFilamentScene();
  utils::EntityManager& oEntityManager = poFilamentEngine->getEntityManager();
  // Ideally this is changed to create all entities on the first go, then
  // we pass them through, upon use this failed in filament engine, more R&D
  // needed
  // oEntitymanager.create(shapes.size(), lstEntities);

  for (auto& shape : *shapes) {
    std::shared_ptr<Entity> oEntity = std::make_shared<Entity>(oEntityManager.create());

    shape->bInitAndCreateShape(poFilamentEngine, oEntity);

    poFilamentScene->addEntity(*oEntity);

    // Save Filament entity ID to our entity
    shape->_fEntity = oEntity;

    spdlog::trace("Adding entity {} with filament entity {}",
                  shape->GetGuid(), oEntity->getId());

    // To investigate a better system for implementing layer mask
    // across dart to here.
    // auto& rcm = poFilamentEngine->getRenderableManager();
    // auto instance = rcm.getInstance(*oEntity.get());
    // To investigate
    // rcm.setLayerMask(instance, 0xff, 0x00);

    std::shared_ptr<BaseShape> sharedPtr = std::move(shape);

    m_mapszoShapes.insert(std::pair(sharedPtr->GetGuid(), sharedPtr));

    ecs->addEntity(sharedPtr);
  }

  SPDLOG_TRACE("--{}", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vOnInitSystem() {
  vRegisterMessageHandler(
      ECSMessageType::ToggleShapesInScene, [this](const ECSMessage& msg) {
        spdlog::debug("ToggleShapesInScene");

        const auto value =
            msg.getData<bool>(ECSMessageType::ToggleShapesInScene);

        vToggleAllShapesInScene(value);

        spdlog::debug("ToggleShapesInScene Complete");
      });

  vRegisterMessageHandler(
      ECSMessageType::SetShapeTransform, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("SetShapeTransform");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::SetShapeTransform);

        const auto position =
            msg.getData<filament::math::float3>(ECSMessageType::Position);
        const auto rotation =
            msg.getData<filament::math::quatf>(ECSMessageType::Rotation);
        const auto scale =
            msg.getData<filament::math::float3>(ECSMessageType::Scale);

        // find the entity in our list:
        if (const auto ourEntity = m_mapszoShapes.find(guid);
            ourEntity != m_mapszoShapes.end()) {
          const auto entity = ourEntity->second;
          const auto baseTransform = dynamic_cast<BaseTransform*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          const auto collidable = dynamic_cast<Collidable*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<Collidable>())
                  .get());

          // this ideally checks for SetShouldMatchAttachedObject in the future
          // - todo
          baseTransform->SetPosition(position);
          baseTransform->SetRotation(rotation);
          baseTransform->SetScale(scale);

          EntityTransforms::vApplyTransform(*(entity->poGetEntity()),
                                            *baseTransform);
        }

        SPDLOG_TRACE("SetShapeTransform Complete");
      });

  vRegisterMessageHandler(
      ECSMessageType::ToggleVisualForEntity, [this](const ECSMessage& msg) {
        spdlog::debug("ToggleVisualForEntity");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ToggleVisualForEntity);
        const auto value = msg.getData<bool>(ECSMessageType::BoolValue);

        vToggleSingleShapeInScene(guid, value);

        spdlog::debug("ToggleVisualForEntity Complete");
      });

  // ChangeTranslationByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeTranslationByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeTranslationByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeTranslationByGUID);

        const auto position =
            msg.getData<filament::math::float3>(ECSMessageType::floatVec3);

        // find the entity in our list:
        if (const auto ourEntity = m_mapszoShapes.find(guid);
            ourEntity != m_mapszoShapes.end()) {
          const auto entity = ourEntity->second;
          const auto baseTransform = dynamic_cast<BaseTransform*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          const auto collidable = dynamic_cast<Collidable*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<Collidable>())
                  .get());

          // this ideally checks for SetShouldMatchAttachedObject in the future
          // - todo
          baseTransform->SetPosition(position);
          // collidable->SetCenterPoint(position);

          EntityTransforms::vApplyTransform(*(entity->poGetEntity()),
                                            *baseTransform);
        }

        SPDLOG_TRACE("ChangeTranslationByGUID Complete");
      });

  // ChangeRotationByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeRotationByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeRotationByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeRotationByGUID);

        const auto values =
            msg.getData<filament::math::float4>(ECSMessageType::floatVec4);
        filament::math::quatf rotation(values);

        // find the entity in our list:
        if (const auto ourEntity = m_mapszoShapes.find(guid);
            ourEntity != m_mapszoShapes.end()) {
          const auto entity = ourEntity->second;
          const auto baseTransform = dynamic_cast<BaseTransform*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          // change stuff.
          baseTransform->SetRotation(rotation);

          EntityTransforms::vApplyTransform(*(entity->poGetEntity()),
                                            *baseTransform);
        }

        SPDLOG_TRACE("ChangeRotationByGUID Complete");
      });

  // ChangeScaleByGUID
  vRegisterMessageHandler(
      ECSMessageType::ChangeScaleByGUID, [this](const ECSMessage& msg) {
        SPDLOG_TRACE("ChangeScaleByGUID");

        const auto guid =
            msg.getData<EntityGUID>(ECSMessageType::ChangeScaleByGUID);

        const auto values =
            msg.getData<filament::math::float3>(ECSMessageType::floatVec3);

        // find the entity in our list:
        if (const auto ourEntity = m_mapszoShapes.find(guid);
            ourEntity != m_mapszoShapes.end()) {
          const auto entity = ourEntity->second;
          const auto baseTransform = dynamic_cast<BaseTransform*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<BaseTransform>())
                  .get());

          const auto collidable = dynamic_cast<Collidable*>(
              entity
                  ->GetComponent(Component::StaticGetTypeID<Collidable>())
                  .get());

          // this ideally checks for SetShouldMatchAttachedObject in the future
          // - todo
          collidable->SetExtentsSize(values);
          baseTransform->SetScale(values);

          EntityTransforms::vApplyTransform(*(entity->poGetEntity()),
                                            *baseTransform);
        }

        SPDLOG_TRACE("ChangeScaleByGUID Complete");
      });
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vUpdate(float /*fElapsedTime*/) {}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::vShutdownSystem() {
  // remove all filament entities.
  vRemoveAllShapesInScene();
}

////////////////////////////////////////////////////////////////////////////////////
void ShapeSystem::DebugPrint() {
  SPDLOG_DEBUG("{}", __FUNCTION__);
}
}  // namespace plugin_filament_view
