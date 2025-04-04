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
#include "entityobject.h"

#include <core/components/derived/animation.h>
#include <core/components/derived/light.h>
#include <core/include/literals.h>
#include <core/systems/derived/animation_system.h>
#include <core/systems/derived/light_system.h>
#include <core/systems/ecs.h>
#include <core/utils/uuidGenerator.h>
#include <core/utils/deserialize.h>
#include <plugins/common/common.h>
#include <utility>

namespace plugin_filament_view {

EntityObject::EntityObject() : guid_(generateUUID()), name_(std::string()) {}

EntityObject::EntityObject(std::string name)
    : guid_(generateUUID()), name_(std::move(name)) {}

EntityObject::EntityObject(EntityGUID guid)
    : guid_(guid), name_(std::string()) {}

EntityObject::EntityObject(std::string name, EntityGUID guid)
    : guid_(std::move(guid)), name_(std::move(name)) {}

EntityObject::EntityObject(const EntityDescriptor& descriptor)
    : guid_(descriptor.guid), name_(descriptor.name) {}

EntityObject::EntityObject(const flutter::EncodableMap& params) {
  const auto descriptor = DeserializeNameAndGuid(params);
  guid_ = descriptor.guid;
  name_ = descriptor.name;

  assert(guid_ != kNullGuid);
}

/////////////////////////////////////////////////////////////////////////////////////////
EntityDescriptor EntityObject::DeserializeNameAndGuid(
    const flutter::EncodableMap& params) {
  EntityDescriptor descriptor;

  // Deserialize name
  if (const auto itName = params.find(flutter::EncodableValue(kName));
      itName != params.end() && !itName->second.IsNull()) {
    // they're requesting entity be named what they want.

    if (auto requestedName = std::get<std::string>(itName->second);
        !requestedName.empty()) {
      descriptor.name = requestedName;
    }
  }


  // Deserialize guid
  spdlog::debug("Deserializing guid");
  Deserialize::DecodeParameterWithDefaultInt64(kGuid, &descriptor.guid,
                                                 params, kNullGuid);

  if (descriptor.guid == kNullGuid) {
    spdlog::warn("Failed to deserialize guid, generating new one");
    descriptor.guid = generateUUID();
  }

  return descriptor;
}

/////////////////////////////////////////////////////////////////////////////////////////
void EntityObject::vDebugPrintComponents() const {
  spdlog::debug("EntityObject Name \'{}\' UUID {} ComponentCount {}", name_,
                guid_, components_.size());

  for (const auto& component : components_) {
    spdlog::debug("\tComponent Type \'{}\' Name \'{}\'",
                  component->GetTypeName(), component->GetName());
    component->DebugPrint("\t\t");
  }
}

/////////////////////////////////////////////////////////////////////////////////////////
void EntityObject::vUnregisterEntity() {
  if (!m_bAlreadyRegistered) {
    return;
  }

  const auto ecs = ECSManager::GetInstance();

  ecs->removeEntity(guid_);

  m_bAlreadyRegistered = false;
}

/////////////////////////////////////////////////////////////////////////////////////////
void EntityObject::vRegisterEntity() {
  if (m_bAlreadyRegistered) {
    return;
  }

  const auto ecs = ECSManager::GetInstance();
  ecs->addEntity(shared_from_this());

  m_bAlreadyRegistered = true;
}

/////////////////////////////////////////////////////////////////////////////////////////
void EntityObject::vShallowCopyComponentToOther(size_t staticTypeID,
                                                EntityObject& other) const {
  const auto component = GetComponentByStaticTypeID(staticTypeID);
  if (component == nullptr) {
    spdlog::warn("Unable to clone component of {}", staticTypeID);
    return;
  }

  other.vAddComponent(std::shared_ptr<Component>(component->Clone()));
}

/////////////////////////////////////////////////////////////////////////////////////////
void EntityObject::vAddComponent(std::shared_ptr<Component> component,
                                 const bool bAutoAddToSystems) {
  component->entityOwner_ = this;

  if (bAutoAddToSystems) {
    if (component->GetTypeID() == Component::StaticGetTypeID<Light>()) {
      const auto lightSystem =
          ECSManager::GetInstance()->getSystem<LightSystem>(
              __FUNCTION__);

      lightSystem->vRegisterEntityObject(shared_from_this());
    }

    if (component->GetTypeID() == Component::StaticGetTypeID<Animation>()) {
      const auto animationSystem =
          ECSManager::GetInstance()->getSystem<AnimationSystem>(
              "loadModelGltf");

      animationSystem->vRegisterEntityObject(shared_from_this());
    }
  }

  components_.emplace_back(std::move(component));
}

}  // namespace plugin_filament_view