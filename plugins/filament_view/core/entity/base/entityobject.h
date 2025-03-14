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

/// @brief EntityGUID is a type alias for the GUID of an entity (currently an int64_t).
using EntityGUID = int64_t;

/// @brief kNullGuid is a constant that represents a null GUID.
constexpr EntityGUID kNullGuid = 0;

/// @brief EntityDescriptor is a struct that holds the name and guid of an entity.
struct EntityDescriptor {
  std::string name = "";
  EntityGUID guid = kNullGuid;
};

/// @brief EntityObject is the base class for all entities in the system.
class EntityObject : public std::enable_shared_from_this<EntityObject> {
  friend class CollisionSystem;
  friend class MaterialSystem;
  friend class ModelSystem;
  friend class ShapeSystem;
  friend class AnimationSystem;
  friend class LightSystem;
  friend class SceneTextDeserializer;

 public:
  // Overloading the == operator to compare based on guid_
  bool operator==(const EntityObject& other) const {
    return guid_ == other.guid_;
  }

  // Pass in the <DerivedClass>::StaticGetTypeID()
  // Returns true if valid, false if not found.
  [[nodiscard]] bool HasComponentByStaticTypeID(size_t staticTypeID) const {
    return std::any_of(components_.begin(), components_.end(),
                       [staticTypeID](const auto& item) {
                         return item->GetTypeID() == staticTypeID;
                       });
  }

  [[nodiscard]] EntityGUID GetGuid() const {
    return guid_;
  }

  EntityObject(const EntityObject&) = delete;
  EntityObject& operator=(const EntityObject&) = delete;

  // This will register the object to the entity object locator
  // system. Components will be registered with their respective
  // System when a component is added to the entity.
  void vRegisterEntity();

  // Called from destructor but if saved in other list, which it
  // will be. then it won't unregister. So you need to call this
  // when you want it gone.
  void vUnregisterEntity();

 protected:
  /// @brief Constructor for EntityObject. Generates a GUID and has an empty name.
  explicit EntityObject();
  /// @brief Constructor for EntityObject with a name. Generates a unique GUID.
  explicit EntityObject(std::string name);
  /// @brief Constructor for EntityObject with GUID. Name is empty.
  explicit EntityObject(EntityGUID guid);
  /// @brief Constructor for EntityObject with a name and GUID.
  EntityObject(std::string name, EntityGUID guid);
  /// @brief Constructor for EntityObject based on an EntityDescriptor, containing a name and GUID.
  explicit EntityObject(const EntityDescriptor& descriptor);
  /// @brief Constructor for EntityObject based on an EncodableMap. Deserializes the name and GUID.
  explicit EntityObject(const flutter::EncodableMap& params);

  virtual ~EntityObject() {
    vUnregisterEntity();

    // smart ptrs in components deleted on clear.
    components_.clear();
  }

  virtual void DebugPrint() const = 0;

  [[nodiscard]] const std::string& GetName() const { return name_; }

  void vAddComponent(std::shared_ptr<Component> component,
                     bool bAutoAddToSystems = true);

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

  static EntityDescriptor DeserializeNameAndGuid(const flutter::EncodableMap& params);

 private:
  /// GUID of the entity.
  /// This is used for identifying the entity, and must be unique.
  /// Also used as a key in the entity object locator system.
  EntityGUID guid_;

  /// Name of the entity.
  /// This is used only for debugging and logging purposes.
  /// It is not used for identifying the entity, and does not need to be unique.
  std::string name_;

  bool m_bAlreadyRegistered{};

  // Vector for now, we shouldn't be adding and removing
  // components frequently during runtime.
  //
  // In the future this is probably a map<type, vector_or_list<Comp*>>
  std::vector<std::shared_ptr<Component>> components_;
};
}  // namespace plugin_filament_view