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
#include "entitytransforms.h"


#include <filament/TransformManager.h>
#include <filament/math/TMatHelpers.h>
#include <filament/gltfio/math.h>

#include <core/entity/derived/model/model.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/transform_system.h>
#include <core/systems/ecs.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {


////////////////////////////////////////////////////////////////////////////
filament::math::mat3f EntityTransforms::identity3x3() {
  return {filament::math::float3{1.0f, 0.0f, 0.0f},
          filament::math::float3{0.0f, 1.0f, 0.0f},
          filament::math::float3{0.0f, 0.0f, 1.0f}};
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::identity4x4() {
  return {filament::math::float4(1.0f, 0.0f, 0.0f, 0.0f),
          filament::math::float4(0.0f, 1.0f, 0.0f, 0.0f),
          filament::math::float4(0.0f, 0.0f, 1.0f, 0.0f),
          filament::math::float4(0.0f, 0.0f, 0.0f, 1.0f)};
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::oApplyShear(
    const filament::math::mat4f& matrix,
    const filament::math::float3& shear) {
  // Create a shear matrix
  filament::math::mat4f shearMatrix = identity4x4();

  // Apply shear values (assuming shear.x applies to YZ, shear.y applies to XZ,
  // shear.z applies to XY)
  shearMatrix[1][0] = shear.x;  // Shear along X axis
  shearMatrix[2][0] = shear.y;  // Shear along Y axis
  shearMatrix[2][1] = shear.z;  // Shear along Z axis

  // Multiply the original matrix by the shear matrix
  return matrix * shearMatrix;
}

////////////////////////////////////////////////////////////////////////////
// Functions that use CustomModelViewer to get the engine
void EntityTransforms::vApplyScale(const FilamentEntity& poEntity,
                                   const filament::math::float3& scale) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyScale(poEntity, scale, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyRotation(const FilamentEntity& poEntity,
                                      const filament::math::quatf& rotation) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyRotation(poEntity, rotation, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTranslate(
    const FilamentEntity& poEntity,
    const filament::math::float3& translation) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyTranslate(poEntity, translation, engine);
}


////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyShear(const FilamentEntity& poEntity,
                                   const filament::math::float3& shear) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyShear(poEntity, shear, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vResetTransform(
    const FilamentEntity& poEntity) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vResetTransform(poEntity, engine);
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::oGetCurrentTransform(
    const FilamentEntity& poEntity) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  return oGetCurrentTransform(poEntity, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyLookAt(const FilamentEntity& poEntity,
                                    const filament::math::float3& target,
                                    const filament::math::float3& up) {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyLookAt(poEntity, target, up, engine);
}

////////////////////////////////////////////////////////////////////////////
// Functions that take an engine as a parameter (these already exist)
void EntityTransforms::vApplyScale(const FilamentEntity& poEntity,
                                   const filament::math::float3& scale,
                                   filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Get the current transform
  const auto currentTransform = transformManager.getTransform(instance);

  // Create the scaling matrix
  const auto scalingMatrix = filament::math::mat4f::scaling(scale);

  // Combine the current transform with the scaling matrix
  const auto combinedTransform = currentTransform * scalingMatrix;

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyRotation(const FilamentEntity& poEntity,
                                      const filament::math::quatf& rotation,
                                      filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Get the current transform
  const auto currentTransform = transformManager.getTransform(instance);

  // Convert the quaternion to a 4x4 rotation matrix
  const filament::math::mat4f rotationMat4 = filament::math::mat4f(rotation);

  // Combine the current transform with the rotation matrix
  const auto combinedTransform = currentTransform * rotationMat4;

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTranslate(
    const FilamentEntity& poEntity,
    const filament::math::float3& translation,
    filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Get the current transform
  const auto currentTransform = transformManager.getTransform(instance);

  // Create the translation matrix
  const auto translationMatrix =
      filament::math::mat4f::translation(translation);

  // Combine the current transform with the translation matrix
  const auto combinedTransform = currentTransform * translationMatrix;

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(const FilamentEntity& poEntity,
  const BaseTransform& transform) {

  // Create the rotation, scaling, and translation matrices
  const auto rotation = transform.GetRotation();
  const auto scaling = transform.GetScale();
  const auto translation = transform.GetPosition();

  vApplyTransform(
    poEntity,
    rotation,
    scaling,
    translation,
    // TODO: Get parent from transform
    nullptr
  );
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(
  const FilamentEntity& poEntity,
  const filament::math::quatf& rotation,
  const filament::math::float3& scale,
  const filament::math::float3& translation,
  const FilamentEntity* parent
) {
  if (!poEntity)
    return;

  const auto combinedTransform = filament::gltfio::composeMatrix(
    translation,
    rotation,
    scale
  );

  
  // Set the combined transform back to the entity
  vApplyTransform(poEntity, combinedTransform, parent);
}


////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(const FilamentEntity& poEntity,
  const filament::math::mat4f& transform,
  const FilamentEntity* parent) {
  const auto filamentSystem = ECSManager::GetInstance()->getSystem<FilamentSystem>("EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyTransform(poEntity, transform, parent, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(const FilamentEntity& poEntity,
                                       const filament::math::mat4f& transform,
                                       const FilamentEntity* parent,
                                       filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // If a parent ID is provided, set the parent transform
  if (parent != nullptr) {
    // Get instance of parent
    const auto parentInstance = transformManager.getInstance(*parent);
    // Get current parent
    const auto currentParent = transformManager.getParent(instance);

    // Skip parenting if same as current parent
    if(currentParent == *parent) {
      spdlog::debug("[{}] New parent entity is the same as the current Parent entity ({}), skipping reparenting.",
                    __FUNCTION__, parent->getId());
      return;
    }

    // Check if the parent instance is valid
    if(parentInstance.isValid()) {
      if(instance.asValue() == parentInstance.asValue()) {
        throw std::runtime_error(
          fmt::format("[{}] New parent instance is the same as the current instance ({}), skipping parenting.",
                      __FUNCTION__, instance.asValue()));
      }
    } else {
      throw std::runtime_error(
        fmt::format("[{}] Parent instance ? of {} is not valid.",
                    __FUNCTION__, poEntity.getId()));
    }


    // Set the parent transform
    try {
      // make sure the parent has a valid transform
      const auto parentTransform = transformManager.getTransform(parentInstance);
      assert(parentTransform.isValid());

      transformManager.setParent(instance, parentInstance);
    } catch (const std::exception& e) {
      spdlog::error("[{}] Error setting parent: {}", __FUNCTION__, e.what());
    } catch (...) {
      spdlog::error("[{}] Unknown error setting parent", __FUNCTION__);
    }
  }



  // Set the provided transform matrix directly to the entity
  transformManager.setTransform(instance, transform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyShear(const FilamentEntity& poEntity,
                                   const filament::math::float3& shear,
                                   filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Get the current transform
  const auto currentTransform = transformManager.getTransform(instance);

  // Create the shear matrix
  const auto shearMatrix = oApplyShear(currentTransform, shear);

  // Combine the current transform with the shear matrix
  const auto combinedTransform = currentTransform * shearMatrix;

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vResetTransform(const FilamentEntity& poEntity,
                                       filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Reset the transform to identity
  const auto identityMatrix = identity4x4();
  transformManager.setTransform(instance, identityMatrix);
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::oGetCurrentTransform(
    const FilamentEntity& poEntity,
    filament::Engine* engine) {
  if (!poEntity)
    return identity4x4();

  const auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Return the current transform
  return transformManager.getTransform(instance);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyLookAt(const FilamentEntity& poEntity,
                                    const filament::math::float3& target,
                                    const filament::math::float3& up,
                                    filament::Engine* engine) {
  if (!poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(poEntity);

  // Get the current position of the entity
  auto currentTransform = transformManager.getTransform(instance);
  const filament::math::float3 position = currentTransform[3].xyz;

  // Calculate the look-at matrix
  const auto lookAtMatrix = filament::math::mat4f::lookAt(position, target, up);

  // Set the look-at transform to the entity
  transformManager.setTransform(instance, lookAtMatrix);
}


filament::math::float3 EntityTransforms::transformPositionVector(
    const filament::math::float3& position,
    const filament::math::mat4f& transform) {
  // Apply the transformation matrix to the position vector
  return filament::math::float3(
      transform[0].x * position.x + transform[1].x * position.y +
          transform[2].x * position.z + transform[3].x,
      transform[0].y * position.x + transform[1].y * position.y +
          transform[2].y * position.z + transform[3].y,
      transform[0].z * position.x + transform[1].z * position.y +
          transform[2].z * position.z + transform[3].z);
}

filament::math::float3 EntityTransforms::transformScaleVector(
  const filament::math::float3& scale,
  const filament::math::mat4f& transform
) {
  // Extract the scaling factors from the transformation matrix
  filament::math::float3 scaleFactors = {
    length(transform[0].xyz),
    length(transform[1].xyz),
    length(transform[2].xyz),
  };

  // Apply the scaling factors to the scale vector
  return filament::math::float3(
      scale.x * scaleFactors.x,
      scale.y * scaleFactors.y,
      scale.z * scaleFactors.z
  );
}

}  // namespace plugin_filament_view
