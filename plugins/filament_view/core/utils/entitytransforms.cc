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

#include <core/entity/derived/model/model.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/derived/entityobject_locator_system.h>
#include <core/systems/ecsystems_manager.h>
#include <filament/TransformManager.h>
#include <filament/math/TMatHelpers.h>
#include <plugins/common/common.h>

namespace plugin_filament_view {

using utils::Entity;

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
filament::math::mat3f EntityTransforms::QuaternionToMat3f(
    const filament::math::quatf& rotation) {
  // Extract quaternion components
  const float x = rotation.x;
  const float y = rotation.y;
  const float z = rotation.z;
  const float w = rotation.w;

  // Calculate coefficients
  const float xx = x * x;
  const float yy = y * y;
  const float zz = z * z;
  const float xy = x * y;
  const float xz = x * z;
  const float yz = y * z;
  const float wx = w * x;
  const float wy = w * y;
  const float wz = w * z;

  // Create the 3x3 rotation matrix
  return {filament::math::float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),
                                 2.0f * (xz - wy)),
          filament::math::float3(2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz),
                                 2.0f * (yz + wx)),
          filament::math::float3(2.0f * (xz + wy), 2.0f * (yz - wx),
                                 1.0f - 2.0f * (xx + yy))};
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::QuaternionToMat4f(
    const filament::math::quatf& rotation) {
  // Use the QuaternionToMat3f function to get the 3x3 matrix
  const filament::math::mat3f rotationMatrix = QuaternionToMat3f(rotation);

  // Embed the 3x3 rotation matrix into a 4x4 matrix
  return filament::math::mat4f(rotationMatrix);
}

////////////////////////////////////////////////////////////////////////////
// Functions that use CustomModelViewer to get the engine
void EntityTransforms::vApplyScale(const std::shared_ptr<Entity>& poEntity,
                                   const filament::math::float3& scale) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyScale(poEntity, scale, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyRotation(const std::shared_ptr<Entity>& poEntity,
                                      const filament::math::quatf& rotation) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyRotation(poEntity, rotation, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTranslate(
    const std::shared_ptr<Entity>& poEntity,
    const filament::math::float3& translation) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyTranslate(poEntity, translation, engine);
}


////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyShear(const std::shared_ptr<Entity>& poEntity,
                                   const filament::math::float3& shear) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyShear(poEntity, shear, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vResetTransform(
    const std::shared_ptr<Entity>& poEntity) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vResetTransform(poEntity, engine);
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::oGetCurrentTransform(
    const std::shared_ptr<Entity>& poEntity) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  return oGetCurrentTransform(poEntity, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyLookAt(const std::shared_ptr<Entity>& poEntity,
                                    const filament::math::float3& target,
                                    const filament::math::float3& up) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();
  vApplyLookAt(poEntity, target, up, engine);
}

////////////////////////////////////////////////////////////////////////////
// Functions that take an engine as a parameter (these already exist)
void EntityTransforms::vApplyScale(const std::shared_ptr<Entity>& poEntity,
                                   const filament::math::float3& scale,
                                   filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

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
void EntityTransforms::vApplyRotation(const std::shared_ptr<Entity>& poEntity,
                                      const filament::math::quatf& rotation,
                                      filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Get the current transform
  const auto currentTransform = transformManager.getTransform(instance);

  // Convert the quaternion to a 4x4 rotation matrix
  const filament::math::mat4f rotationMat4 = QuaternionToMat4f(rotation);

  // Combine the current transform with the rotation matrix
  const auto combinedTransform = currentTransform * rotationMat4;

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTranslate(
    const std::shared_ptr<Entity>& poEntity,
    const filament::math::float3& translation,
    filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

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
void EntityTransforms::vApplyTransform(const std::shared_ptr<Entity>& poEntity,
  const filament::math::mat4f& transform) {
const auto filamentSystem =
ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
FilamentSystem::StaticGetTypeID(), "EntityTransforms");
const auto engine = filamentSystem->getFilamentEngine();
vApplyTransform(poEntity, transform, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(const std::shared_ptr<Entity>& poEntity,
  const BaseTransform& transform) {
const auto filamentSystem =
ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
FilamentSystem::StaticGetTypeID(), "EntityTransforms");
const auto engine = filamentSystem->getFilamentEngine();

// Create the rotation, scaling, and translation matrices
const auto rotationMatrix = QuaternionToMat4f(transform.GetRotation());
const auto scalingMatrix =
filament::math::mat4f::scaling(transform.GetScale());
const auto translationMatrix =
filament::math::mat4f::translation(transform.GetPosition());

// Combine the transformations: translate * rotate * scale
const auto combinedTransform =
translationMatrix * rotationMatrix * scalingMatrix;

vApplyTransform(poEntity, combinedTransform, engine);
}

////////////////////////////////////////////////////////////////////////////
// TODO: deprecate
void EntityTransforms::vApplyTransform(
const std::shared_ptr<Entity>& poEntity,
const filament::math::quatf& rotation,
const filament::math::float3& scale,
const filament::math::float3& translation,
const std::shared_ptr<Entity>& filamentEntityParent) {
const auto filamentSystem =
ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
FilamentSystem::StaticGetTypeID(), "EntityTransforms");
const auto engine = filamentSystem->getFilamentEngine();
vApplyTransform(poEntity, rotation, scale, translation, filamentEntityParent, engine);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(
    const std::shared_ptr<Entity>& poEntity,
    const filament::math::quatf& rotation,
    const filament::math::float3& scale,
    const filament::math::float3& translation,
    const std::shared_ptr<Entity>& filamentEntityParent,
    filament::Engine* engine) {


  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Create the rotation, scaling, and translation matrices
  const auto rotationMatrix = QuaternionToMat4f(rotation);
  const auto scalingMatrix = filament::math::mat4f::scaling(scale);
  const auto translationMatrix =
      filament::math::mat4f::translation(translation);

  // Combine the transformations: translate * rotate * scale
  const auto combinedTransform =
      translationMatrix * rotationMatrix * scalingMatrix;

  

  // Set the combined transform back to the entity
  transformManager.setTransform(instance, combinedTransform);


  // If a parent ID is provided, set the parent transform
  if (filamentEntityParent != nullptr) {
    // Get instance of parent
    const auto parentInstance = transformManager.getInstance(*filamentEntityParent);

    // Check if the parent instance is valid
    if(parentInstance.isValid()) {
      if(instance.asValue() == parentInstance.asValue()) {
        throw std::runtime_error(
          fmt::format("[{}] Parent instance is the same as the current instance ({}), skipping parenting.",
                      __FUNCTION__, instance.asValue()));
      }
    } else {
      throw std::runtime_error(
        fmt::format("[{}] Parent instance ? of {} is not valid.",
                    __FUNCTION__, poEntity->getId()));
    }

    // Set the parent transform
    try {
      // get parent transform
      const auto parentTransform = transformManager.getTransform(parentInstance);

      transformManager.setParent(instance, parentInstance);
    } catch (const std::exception& e) {
      spdlog::error("[{}] Error setting parent: {}", __FUNCTION__, e.what());
    } catch (...) {
      spdlog::error("[{}] Unknown error setting parent", __FUNCTION__);
    }

  }
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(const std::shared_ptr<Entity>& poEntity,
                                       const filament::math::mat4f& transform,
                                       filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Set the provided transform matrix directly to the entity
  transformManager.setTransform(instance, transform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyShear(const std::shared_ptr<Entity>& poEntity,
                                   const filament::math::float3& shear,
                                   filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

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
void EntityTransforms::vResetTransform(const std::shared_ptr<Entity>& poEntity,
                                       filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Reset the transform to identity
  const auto identityMatrix = identity4x4();
  transformManager.setTransform(instance, identityMatrix);
}

////////////////////////////////////////////////////////////////////////////
filament::math::mat4f EntityTransforms::oGetCurrentTransform(
    const std::shared_ptr<Entity>& poEntity,
    filament::Engine* engine) {
  if (!*poEntity)
    return identity4x4();

  const auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Return the current transform
  return transformManager.getTransform(instance);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyLookAt(const std::shared_ptr<Entity>& poEntity,
                                    const filament::math::float3& target,
                                    const filament::math::float3& up,
                                    filament::Engine* engine) {
  if (!*poEntity)
    return;

  auto& transformManager = engine->getTransformManager();
  const auto instance = transformManager.getInstance(*poEntity);

  // Get the current position of the entity
  auto currentTransform = transformManager.getTransform(instance);
  const filament::math::float3 position = currentTransform[3].xyz;

  // Calculate the look-at matrix
  const auto lookAtMatrix = filament::math::mat4f::lookAt(position, target, up);

  // Set the look-at transform to the entity
  transformManager.setTransform(instance, lookAtMatrix);
}

////////////////////////////////////////////////////////////////////////////
// HAS PARENTING!
void EntityTransforms::vApplyTransform(
    const std::shared_ptr<Model>& oModelAsset,
    const BaseTransform& transform,
    filament::Engine* engine) {
  auto& transformManager = engine->getTransformManager();

  // Get filament entity
  Entity entityToWorkWith;
  if (oModelAsset->getAsset() != nullptr) {
    entityToWorkWith = oModelAsset->getAsset()->getRoot();
  } else if (oModelAsset->getAssetInstance() != nullptr) {
    entityToWorkWith = oModelAsset->getAssetInstance()->getRoot();
  } else {
    throw std::runtime_error(
        "[EntityTransforms::vApplyTransform] No entity to work with???");
  }
  const auto ei = transformManager.getInstance(entityToWorkWith);

  // Create the rotation, scaling, and translation matrices
  const auto rotationMatrix = QuaternionToMat4f(transform.GetRotation());
  const auto scalingMatrix =
      filament::math::mat4f::scaling(transform.GetScale());
  const auto translationMatrix =
      filament::math::mat4f::translation(transform.GetPosition());

  // Combine the transformations: translate * rotate * scale
  const auto combinedTransform =
      translationMatrix * rotationMatrix * scalingMatrix;

  // Set the combined transform back to the entity
  transformManager.setTransform(ei, combinedTransform);
}

////////////////////////////////////////////////////////////////////////////
void EntityTransforms::vApplyTransform(
    const std::shared_ptr<Model>& oModelAsset,
    const BaseTransform& transform) {
  const auto filamentSystem =
      ECSystemManager::GetInstance()->poGetSystemAs<FilamentSystem>(
          FilamentSystem::StaticGetTypeID(), "EntityTransforms");
  const auto engine = filamentSystem->getFilamentEngine();

  vApplyTransform(oModelAsset, transform, engine);
}

////////////////////////////////////////////////////////////////////////////
filament::math::float3 EntityTransforms::oGetTranslationFromTransform(
    const filament::math::mat4f& transform) {
  return transform[3].xyz;
}

////////////////////////////////////////////////////////////////////////////
filament::math::float3 EntityTransforms::oGetScaleFromTransform(
    const filament::math::mat4f& transform) {
  throw std::runtime_error("Not implemented");
}

////////////////////////////////////////////////////////////////////////////
filament::math::quatf EntityTransforms::oGetRotationFromTransform(
    const filament::math::mat4f& transform) {
  throw std::runtime_error("Not implemented");
}

}  // namespace plugin_filament_view
