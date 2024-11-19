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

#include <encodable_value.h>
#include <string>
#include <vector>

#include <core/components/base/component.h>
#include <core/components/derived/material_definitions.h>

namespace plugin_filament_view {
class MaterialParameter;

using EntityGUID = std::string;

class EntityObject {
  friend class CollisionSystem;
  friend class MaterialSystem;
  friend class ModelSystem;
  friend class AnimationSystem;
  friend class LightSystem;
  friend class SceneTextDeserializer;

 public:
  // Overloading the == operator to compare based on global_guid_
  bool operator==(const EntityObject& other) const {
    return global_guid_ == other.global_guid_;
  }

  // Pass in the <DerivedClass>::StaticGetTypeID()
  // Returns true if valid, false if not found.
  [[nodiscard]] bool HasComponentByStaticTypeID(size_t staticTypeID) const {
    return std::any_of(components_.begin(), components_.end(),
                       [staticTypeID](const auto& item) {
                         return item->GetTypeID() == staticTypeID;
                       });
  }

  [[nodiscard]] const std::string& GetGlobalGuid() const {
    return global_guid_;
  }

  EntityObject(const EntityObject&) = delete;
  EntityObject& operator=(const EntityObject&) = delete;

 protected:
  explicit EntityObject(std::string name);
  // Note, global_guid, needs to be unique here; this is mainly here for
  // creating objects that are GUID created in non-native code.
  EntityObject(std::string name, std::string global_guid);

  virtual ~EntityObject() {
    // smart ptrs in components deleted on clear.
    components_.clear();
  }

  virtual void DebugPrint() const = 0;

  [[nodiscard]] const std::string& GetName() const { return name_; }

  void vAddComponent(std::shared_ptr<Component> component) {
    component->entityOwner_ = this;
    components_.emplace_back(std::move(component));
  }

  void vRemoveComponent(size_t staticTypeID) {
    components_.erase(std::remove_if(components_.begin(), components_.end(),
                                     [&](auto& item) {
                                       return item->GetTypeID() == staticTypeID;
                                     }),
                      components_.end());
  }

  // Pass in the <DerivedClass>::StaticGetTypeID()
  // Returns component if valid, nullptr if not found.
  [[nodiscard]] std::shared_ptr<Component> GetComponentByStaticTypeID(
      size_t staticTypeID) {
    for (const auto& item : components_) {
      if (item->GetTypeID() == staticTypeID) {
        return item;
      }
    }
    return nullptr;
  }

  [[nodiscard]] std::shared_ptr<Component> GetComponentByStaticTypeID(
      size_t staticTypeID) const {
    for (const auto& item : components_) {
      if (item->GetTypeID() == staticTypeID) {
        return item;
      }
    }
    return nullptr;
  }

  void vDebugPrintComponents() const;

  // finds the size_t staticTypeID in the component list
  // and creates a copy and assigns to the others list
  void vShallowCopyComponentToOther(size_t staticTypeID,
                                    EntityObject& other) const;

  void DeserializeNameAndGlobalGuid(const flutter::EncodableMap& params);

 private:
  EntityGUID global_guid_;
  std::string name_;

  // Look, if you're calling this, its expected your name clashing checking
  // yourself. This isn't done for you. Please dont have 100 'my_sphere'.
  // You're gonna have a bad time.
  // This is currently called on objection deserialization from non-native
  // code where they are in control of the naming of objects for easier
  // use.
  void vOverrideName(const std::string& name);
  void vOverrideGlobalGuid(const std::string& global_guid);

  // Vector for now, we shouldn't be adding and removing
  // components frequently during runtime.
  //
  // In the future this is probably a map<type, vector_or_list<Comp*>>
  std::vector<std::shared_ptr<Component>> components_;
};
}  // namespace plugin_filament_view