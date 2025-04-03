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

#include <core/entity/base/entityobject.h>
#include <core/systems/base/ecsystem.h>
#include <core/utils/kvtree.h>
#include <map>

namespace plugin_filament_view {


class EntityObjectLocatorSystem : public ECSystem {
 public:
  EntityObjectLocatorSystem() = default;

  // Disallow copy and assign.
  EntityObjectLocatorSystem(const EntityObjectLocatorSystem&) = delete;
  EntityObjectLocatorSystem& operator=(const EntityObjectLocatorSystem&) =
      delete;

  void vInitSystem() override;
  void vUpdate(float fElapsedTime) override;
  void vShutdownSystem() override;
  void DebugPrint() override;

  [[nodiscard]] std::shared_ptr<EntityObject> poGetEntityObjectById(
      EntityGUID id) const;

  // TODO: move those to ECSManager
  /// Moves the entity with the given GUID to the parent with the given GUID.
  void ReparentEntityObject(const std::shared_ptr<EntityObject>& entity,
                             const EntityGUID& parentGuid);
  /// Returns the children of the entity with the given GUID.
  [[nodiscard]] std::vector<std::shared_ptr<EntityObject>> GetEntityChildren(
      EntityGUID id) const;
  /// Returns the GUIDs of the children of the entity with the given GUID.
  [[nodiscard]] std::vector<EntityGUID> GetEntityChildrenGuids(
      EntityGUID id) const;

  /// Returns the parent of the entity with the given GUID.
  [[nodiscard]] std::shared_ptr<EntityObject> GetEntityParent(
      EntityGUID id) const;
  /// Returns the GUID of the parent of the entity with the given GUID.
  [[nodiscard]] std::optional<EntityGUID> GetEntityParentGuid(EntityGUID id) const;


  // TODO: refactor these
  void vRegisterEntityObject(const std::shared_ptr<EntityObject>& entity,
    const EntityGUID* parentGuid = nullptr);
  void vUnregisterEntityObject(const std::shared_ptr<EntityObject>& entity);

 private:
  KVTree<EntityGUID, std::shared_ptr<EntityObject>> _entities;
};

}  // namespace plugin_filament_view