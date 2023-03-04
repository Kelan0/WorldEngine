#include "core/engine/renderer/RenderComponent.h"
#include "core/application/Engine.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/Material.h"

RenderComponent::RenderComponent():
    RenderComponent(UpdateType_Dynamic, UpdateType_Static, UpdateType_Static) {
}

RenderComponent::RenderComponent(const UpdateType& transformUpdateType,
                const UpdateType& materialUpdateType,
                const UpdateType& meshUpdateType):
        m_mesh(nullptr),
        m_material(nullptr),
//        m_materialIndex(0), // missing material
//        m_entityIndex(EntityChangeTracker::INVALID_INDEX),
        m_transformUpdateType(transformUpdateType),
        m_materialUpdateType(materialUpdateType),
        m_meshUpdateType(meshUpdateType) {
}

RenderComponent& RenderComponent::setMesh(const std::shared_ptr<Mesh>& mesh) {
    if (mesh.get() != m_mesh.get()) {
//        Engine::sceneRenderer()->notifyMeshChanged(m_meshUpdateType);
        m_mesh = mesh;
    }
    return *this;
}

RenderComponent& RenderComponent::setMaterial(const std::shared_ptr<Material>& material) {
    if (material == nullptr && m_material == nullptr) {
        return *this; // Old and new material is both null, return
    }

    if (material != nullptr && m_material != nullptr) {
        if (material->getResourceId() == m_material->getResourceId()) {
            return *this; // Material is unchanged, return
        }
    }

    m_material = material;

//    uint32_t materialIndex = Engine::sceneRenderer()->registerMaterial(material.get());
//    if (materialIndex != m_materialIndex) {
//        Engine::sceneRenderer()->notifyMaterialChanged(m_entityIndex);
//        m_materialIndex = materialIndex;
//        m_material = material;
//    }

    return *this;
}

const std::shared_ptr<Mesh>& RenderComponent::mesh() const {
    return m_mesh;
}

const std::shared_ptr<Material>& RenderComponent::material() const {
    return m_material;
}

RenderComponent::UpdateType RenderComponent::transformUpdateType() const {
    return m_transformUpdateType;
}

RenderComponent::UpdateType RenderComponent::materialUpdateType() const {
    return m_materialUpdateType;
}

RenderComponent::UpdateType RenderComponent::meshUpdateType() const {
    return m_meshUpdateType;
}

//void RenderComponent::reindex(RenderComponent& renderComponent, const EntityChangeTracker::entity_index& newEntityIndex) {
//    if (newEntityIndex == renderComponent.m_entityIndex)
//        return;
//    renderComponent.m_entityIndex = newEntityIndex;
//}
//
//const uint32_t& RenderComponent::getMaterialIndex() const {
//    return m_materialIndex;
//}
