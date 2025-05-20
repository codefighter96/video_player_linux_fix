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

#include <core/systems/base/ecsystem.h>
#include <core/components/derived/basetransform.h>
#include <core/utils/vectorutils.h>


namespace plugin_filament_view {

/**
  * TransformSystem is responsible for updating the transforms of entities in the scene.
  * It handles:
  * - Updating the global transforms after local transforms have been modified
  * - Updating Filament's parent tree based on the ECS hierarchy
  * - TODO: Asynchronous interpolation of transforms 
  */

class TransformSystem : public ECSystem {
  public:
    TransformSystem() = default;

    void vOnInitSystem() override;
    void vProcessMessages() override;
    void vShutdownSystem() override;
    void vHandleMessage(const ECSMessage& msg) override;
    void DebugPrint() override;


    void vUpdate(float deltaTime) override {
      //   Filament transform transaction:
      // updating the transforms and the parent tree can be
      // quite expensive, so we want to batch them
      tm->openLocalTransformTransaction();

      // interpolateTransforms(deltaTime);
      updateTransforms();
      updateFilamentParentTree();

      // committing calculates the final global transforms
      tm->commitLocalTransformTransaction();
    }
  
  protected:
    filament::TransformManager* tm = nullptr;
    //
    // Internal logic
    //

    /**
      * Updates the transforms of all entities in the scene.
      *
      * For each transform marked as "dirty", it commits the transform changes
      * to the Filament engine. This includes updating the local transforms
      * and updating the global ones wherever needed.
      */
    void updateTransforms();
    /**
      * Performs reparenting of the Filament parent tree.
      *
      * Iterates on all transforms, updating the Filament parent tree
      * in event of hierarchy changes.
      */
    void updateFilamentParentTree();

    /**
      * TODO: implement this
      * Performs "async" lerp on transforms.
      *
      * For each transform marked as "animating", it performs a lerp step
      * towards the target transform.
      */
    // void interpolateTransforms(float deltaTime);

  public:
    
    /// Applies the transform to the entity with the given ID.
    ///
    /// \param entityId The ID of the entity to apply the transform to.
    /// \param forceRecalculate If true, forces a recalculation of the transform
    /// even if it is not marked as dirty.
    void applyTransform(
      const EntityGUID entityId,
      const bool forceRecalculate = false
    );

    void applyTransform(
      BaseTransform& transform,
      const bool forceRecalculate = false
    );

    void setParent(
      const EntityObject& entity,
      const EntityObject& parent
    );

    void setParent(
      const FilamentEntity& child,
      const FilamentEntity* parent = nullptr
    );
};

}