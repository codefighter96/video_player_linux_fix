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

#include "baseshape.h"

#include <core/include/literals.h>
#include <core/systems/derived/filament_system.h>
#include <core/systems/ecs.h>
#include <core/utils/deserialize.h>
#include <core/utils/entitytransforms.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <math/norm.h>
#include <math/vec3.h>
#include <plugins/common/common.h>

namespace plugin_filament_view::shapes {

using filament::Aabb;
using filament::IndexBuffer;
using filament::RenderableManager;
using filament::VertexAttribute;
using filament::VertexBuffer;
using filament::math::float3;
using filament::math::mat3f;
using filament::math::mat4f;
using filament::math::packSnorm16;
using filament::math::short4;
using utils::Entity;

////////////////////////////////////////////////////////////////////////////

void BaseShape::deserializeFrom(const flutter::EncodableMap& params) {
  RenderableEntityObject::deserializeFrom(params);

  // shapeType
  Deserialize::DecodeEnumParameterWithDefault(kShapeType, &type_, params, ShapeType::Unset);

  // normal
  Deserialize::DecodeParameterWithDefault(kNormal, &m_f3Normal, params, float3(0, 0, 0));

  // isDoubleSided
  m_bDoubleSided = Deserialize::DecodeParameterWithDefault<bool>(kDoubleSided, params, false);

  // MaterialDefinitions (optional)
  if(Deserialize::HasKey(params, kMaterial)) {
    auto matdefParams = std::get<flutter::EncodableMap>(
      params.find(flutter::EncodableValue(kMaterial))->second
    );
    _tmpMaterialDefinitions = std::make_shared<MaterialDefinitions>(matdefParams);
  } else {
    spdlog::debug("This entity params has no material definitions");
  }
}

void BaseShape::onInitialize() {
  RenderableEntityObject::onInitialize();

  checkInitialized();
  spdlog::debug("BaseShape::onInitialize");
  spdlog::debug("BaseShape (id {})", guid_);

  // MaterialDefinitions (optional)
  if(_tmpMaterialDefinitions != nullptr) {
    spdlog::debug("addComponent MaterialDefinitions");
    _tmpMaterialDefinitions->DebugPrint("MaterialDefinitions");
    ecs->addComponent(guid_, std::move(_tmpMaterialDefinitions));
  }

  // TODO: setup renderable & children, from ShapeSystem::...?

  SPDLOG_TRACE("--{}", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////
BaseShape::~BaseShape() {
  vRemoveEntityFromScene();
  vDestroyBuffers();
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::vDestroyBuffers() {
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "BaseShape::vDestroyBuffers");
  const auto filamentEngine = filamentSystem->getFilamentEngine();

  if (m_poMaterialInstance.getStatus() == Status::Success &&
      m_poMaterialInstance.getData() != nullptr) {
    filamentEngine->destroy(m_poMaterialInstance.getData().value());
    m_poMaterialInstance =
        Resource<filament::MaterialInstance*>::Error("Unset");
  }

  if (m_poVertexBuffer) {
    filamentEngine->destroy(m_poVertexBuffer);
    m_poVertexBuffer = nullptr;
  }
  if (m_poIndexBuffer) {
    filamentEngine->destroy(m_poIndexBuffer);
    m_poIndexBuffer = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////
// Unique that we don't want to copy all components, as shapes can have
// collidables, which would make a cascading collidable chain
// NOTE: We also don't copy material definitions. (Purposefully)
void BaseShape::CloneToOther(BaseShape& other) const {
  other.m_f3Normal = m_f3Normal;
  other.m_bDoubleSided = m_bDoubleSided;
  other.m_bIsWireframe = m_bIsWireframe;
  other.type_ = type_;
  other.m_bHasTexturedMaterial = m_bHasTexturedMaterial;

  // and now components.
  this->vShallowCopyComponentToOther(Component::StaticGetTypeID<BaseTransform>(), other);
  this->vShallowCopyComponentToOther(Component::StaticGetTypeID<CommonRenderable>(),
                                     other);

  const std::shared_ptr<Component> componentBT =
      GetComponent(Component::StaticGetTypeID<BaseTransform>());
  const std::shared_ptr<BaseTransform> baseTransformPtr =
      std::dynamic_pointer_cast<BaseTransform>(componentBT);

  const std::shared_ptr<Component> componentCR =
      GetComponent(Component::StaticGetTypeID<CommonRenderable>());
  const std::shared_ptr<CommonRenderable> commonRenderablePtr =
      std::dynamic_pointer_cast<CommonRenderable>(componentCR);

  /// TODO: ecs->addComponent
}


////////////////////////////////////////////////////////////////////////////
void BaseShape::vBuildRenderable(filament::Engine* engine_) {
  checkInitialized();
  // material_manager can and will be null for now on wireframe creation.

  filament::math::float3 aabb;
  spdlog::debug("Building shape '{}'({}) with AABB",
                GetName(), GetGuid());

  const auto transform = GetComponent<BaseTransform>();

  // If we have a collidable, we need to set the AABB to its extent size
  // otherwise we use transform's scale
  if (HasComponent<Collidable>()) {
    const auto collidable = GetComponent<Collidable>();
    aabb = collidable->GetExtentsSize();
    spdlog::debug("Has collidable");
  } else {
    aabb = transform->GetScale();
    spdlog::debug("No collidable, using transform scale");
  }

  spdlog::debug("AABB: x={}, y={}, z={}", aabb.x, aabb.y, aabb.z);


  const auto renderable = GetComponent<CommonRenderable>();

  if (m_bIsWireframe) {
    // We might want to have a specific Material for wireframes in the future.
    // m_poMaterialInstance =
    //  material_manager->getMaterialInstance(m_poMaterialDefinitions->get());
    RenderableManager::Builder(1)
        .boundingBox({{}, aabb})
        //.material(0, m_poMaterialInstance.getData().value())
        .geometry(0, RenderableManager::PrimitiveType::LINES, m_poVertexBuffer,
                  m_poIndexBuffer)
        .culling(renderable->IsCullingOfObjectEnabled())
        .receiveShadows(false)
        .castShadows(false)
        .build(*engine_, *m_poEntity);
  } else {
    vLoadMaterialDefinitionsToMaterialInstance();

    RenderableManager::Builder(1)
        .boundingBox({{}, aabb})
        .material(0, m_poMaterialInstance.getData().value())
        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES,
                  m_poVertexBuffer, m_poIndexBuffer)
        .culling(renderable->IsCullingOfObjectEnabled())
        .receiveShadows(renderable->IsReceiveShadowsEnabled())
        .castShadows(renderable->IsCastShadowsEnabled())
        .build(*engine_, *m_poEntity);
  }

  // Get parent entity id
  const auto parentId = transform->GetParentId();

  if(parentId != kNullGuid) {
    // Get the parent entity object
    auto parentEntity = ecs->getEntity(parentId);
    auto parentFilamentEntity = parentEntity->_fEntity;

    EntityTransforms::vApplyTransform(
      *m_poEntity, transform->GetRotation(),
      transform->GetScale(),
      transform->GetPosition(),
      parentFilamentEntity.get()
    );
  } else {
    EntityTransforms::vApplyTransform(
      *m_poEntity, transform->GetRotation(),
      transform->GetScale(),
      transform->GetPosition(),
      nullptr
    );
  }

  // TODO , need 'its done building callback to delete internal arrays data'
  // - note the calls are async built, but doesn't seem to be a method internal
  // to filament for when the building is complete. Further R&D is needed.
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::vRemoveEntityFromScene() const {
  if (m_poEntity == nullptr) {
    SPDLOG_WARN("Attempt to remove uninitialized shape from scene {}",
                __FUNCTION__);
    return;
  }

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "BaseShape::vRemoveEntityFromScene");

  filamentSystem->getFilamentScene()->removeEntities(m_poEntity.get(), 1);
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::vAddEntityToScene() const {
  if (m_poEntity == nullptr) {
    SPDLOG_WARN("Attempt to add uninitialized shape to scene {}", __FUNCTION__);
    return;
  }

  const auto filamentSystem =
      ecs->getSystem<FilamentSystem>(
          "BaseShape::vRemoveEntityFromScene");
  filamentSystem->getFilamentScene()->addEntity(*m_poEntity);
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::DebugPrint() const {
  vDebugPrintComponents();
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::DebugPrint(const char* tag) const {
  spdlog::debug("++++++++ (Shape) ++++++++");
  spdlog::debug("Tag {} Type {} Wireframe {}", tag, static_cast<int>(type_),
                m_bIsWireframe);
  spdlog::debug("Normal: x={}, y={}, z={}", m_f3Normal.x, m_f3Normal.y,
                m_f3Normal.z);

  spdlog::debug("Double Sided: {}", m_bDoubleSided);

  DebugPrint();

  spdlog::debug("-------- (Shape) --------");
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::vChangeMaterialDefinitions(
    const flutter::EncodableMap& params,
    const TextureMap& /*loadedTextures*/) {
  // if we have a materialdefinitions component, we need to remove it
  // and remake / add a new one.
  if (HasComponent(Component::StaticGetTypeID<MaterialDefinitions>())) {
    ecs->removeComponent(guid_, Component::StaticGetTypeID<MaterialDefinitions>());
  }

  // If you want to inspect the params coming in.
  /*for (const auto& [fst, snd] : params) {
      auto key = std::get<std::string>(fst);
      plugin_common::Encodable::PrintFlutterEncodableValue(key.c_str(), snd);
  }*/

  auto materialDefinitions = std::make_shared<MaterialDefinitions>(params);
  ecs->addComponent(guid_, std::move(materialDefinitions));

  m_poMaterialInstance.vReset();

  // then tell material system to load us the correct way once
  // we're deserialized.
  vLoadMaterialDefinitionsToMaterialInstance();

  if (m_poMaterialInstance.getStatus() != Status::Success) {
    spdlog::error(
        "Unable to load material definition to instance, bailing out.");
    return;
  }

  // now, reload / rebuild the material?
  const auto filamentSystem =
      ECSManager::GetInstance()->getSystem<FilamentSystem>(
          "BaseShape::vChangeMaterialDefinitions");

  // If your entity has multiple primitives, you’ll need to call
  // setMaterialInstanceAt for each primitive you want to update.
  auto& renderManager =
      filamentSystem->getFilamentEngine()->getRenderableManager();
  const auto instanceToChange = renderManager.getInstance(*m_poEntity);
  renderManager.setMaterialInstanceAt(instanceToChange, 0,
                                      *m_poMaterialInstance.getData());
}

////////////////////////////////////////////////////////////////////////////
void BaseShape::vChangeMaterialInstanceProperty(
    const MaterialParameter* materialParam,
    const TextureMap& loadedTextures) {
  const auto data = m_poMaterialInstance.getData().value();

  const auto matDefs = dynamic_cast<MaterialDefinitions*>(
      GetComponent(Component::StaticGetTypeID<MaterialDefinitions>()).get());
  if (matDefs == nullptr) {
    return;
  }

  MaterialDefinitions::vApplyMaterialParameterToInstance(data, materialParam,
                                                         loadedTextures);
}

}  // namespace plugin_filament_view::shapes
