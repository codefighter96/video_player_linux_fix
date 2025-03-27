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
#include "entityobject_locator_system.h"

#include <core/systems/ecsystems_manager.h>
#include <plugins/common/common.h>

// NOLINTNEXTLINE
#include <core/utils/kvtree.cc>

namespace plugin_filament_view {
  template class KVTree<EntityGUID, std::shared_ptr<EntityObject>>;

void EntityObjectLocatorSystem::vInitSystem() {}

void EntityObjectLocatorSystem::vUpdate(float /*fElapsedTime*/) {}

void EntityObjectLocatorSystem::vShutdownSystem() {
  _entities.clear();
}

void EntityObjectLocatorSystem::DebugPrint() {
  spdlog::debug("{}", __FUNCTION__);
}

void EntityObjectLocatorSystem::vRegisterEntityObject(
    const std::shared_ptr<EntityObject>& entity,
    const EntityGUID* parentGuid) {
  if (_entities.get(entity->GetGuid())) {
    spdlog::error(
        "[EntityObjectLocatorSystem::vRegisterEntityObject] Entity with GUID "
        "{} already exists",
        entity->GetGuid());
    return;
  }

  _entities.insert(entity->GetGuid(), entity, parentGuid);
}

void EntityObjectLocatorSystem::vUnregisterEntityObject(
    const std::shared_ptr<EntityObject>& entity) {
  EntityGUID guid = entity->GetGuid();
  _entities.remove(guid);

}

std::shared_ptr<EntityObject> EntityObjectLocatorSystem::poGetEntityObjectById(
    EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[EntityObjectLocatorSystem::GetEntityObjectById] Unable to find "
        "entity with id {}",
        id);
    return nullptr;
  }
  return *(node->getValue());
}

void EntityObjectLocatorSystem::ReparentEntityObject(
    const std::shared_ptr<EntityObject>& entity,
    const EntityGUID& parentGuid) {
  try {
    _entities.reparent(entity->GetGuid(), &parentGuid);
  } catch (const std::runtime_error& e) {
    spdlog::error("[EntityObjectLocatorSystem::ReparentEntityObject] {}",
                  e.what());
  }
}

std::vector<EntityGUID> EntityObjectLocatorSystem::GetEntityChildrenGuids(
    EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[EntityObjectLocatorSystem::GetEntityChildrenGuids] Unable to find "
        "entity with id {}",
        id);
    return {};
  }

  std::vector<KVTreeNode<EntityGUID, std::shared_ptr<EntityObject>>*>
      childrenNodes = node->getChildren();

  std::vector<EntityGUID> childrenGuids;
  childrenGuids.reserve(childrenNodes.size());
  for (const auto* childNode : childrenNodes) {
    childrenGuids.push_back(childNode->getKey());
  }

  return childrenGuids;
}

std::vector<std::shared_ptr<EntityObject>>
EntityObjectLocatorSystem::GetEntityChildren(EntityGUID id) const {
  std::vector<EntityGUID> childrenGuids = GetEntityChildrenGuids(id);

  std::vector<std::shared_ptr<EntityObject>> children;
  children.reserve(childrenGuids.size());
  for (const auto& childGuid : childrenGuids) {
    const auto child = poGetEntityObjectById(childGuid);
    if (child) {
      children.push_back(child);
    }
  }

  return children;
}

std::optional<EntityGUID> EntityObjectLocatorSystem::GetEntityParentGuid(
    EntityGUID id) const {
  const auto* node = _entities.get(id);
  if (!node) {
    spdlog::error(
        "[EntityObjectLocatorSystem::GetEntityParentGuid] Unable to find "
        "entity with id {}",
        id);
    return std::nullopt;
  }

  const auto* parentNode = node->getParent();
  if (!parentNode) {
    return std::nullopt;
  }

  return parentNode->getKey();
} 

} // namespace plugin_filament_view