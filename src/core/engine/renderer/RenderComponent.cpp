#include "core/engine/renderer/RenderComponent.h"
#include "core/application/Engine.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/Material.h"

RenderComponent::RenderComponent():
    RenderComponent(UpdateType_Dynamic, UpdateType_Static) {
}

RenderComponent::RenderComponent(const UpdateType& transformUpdateType, const UpdateType& meshUpdateType):
        m_mesh(nullptr),
        m_material(nullptr),
        m_boundingVolume(nullptr),
        m_transformUpdateType(transformUpdateType),
        m_meshUpdateType(meshUpdateType) {
}

RenderComponent& RenderComponent::setMesh(const std::shared_ptr<Mesh>& mesh) {
    m_mesh = mesh;
    return *this;
}

RenderComponent& RenderComponent::setMaterial(const std::shared_ptr<Material>& material) {
    m_material = material;
    return *this;
}

RenderComponent& RenderComponent::setBoundingVolume(BoundingVolume* boundingVolume) {
    m_boundingVolume = boundingVolume;
    return *this;
}

const std::shared_ptr<Mesh>& RenderComponent::getMesh() const {
    return m_mesh;
}

const std::shared_ptr<Material>& RenderComponent::getMaterial() const {
    return m_material;
}

BoundingVolume* RenderComponent::getBoundingVolume() const {
    return m_boundingVolume;
}

RenderComponent::UpdateType RenderComponent::transformUpdateType() const {
    return m_transformUpdateType;
}

RenderComponent::UpdateType RenderComponent::meshUpdateType() const {
    return m_meshUpdateType;
}

