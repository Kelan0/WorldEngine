#include "core/engine/renderer/RenderComponent.h"
#include "core/application/Application.h"
#include "core/engine/renderer/SceneRenderer.h"

RenderComponent::RenderComponent():
    RenderComponent(UpdateType_Dynamic, UpdateType_Static, UpdateType_Static) {
}

RenderComponent::RenderComponent(const UpdateType& transformUpdateType,
                const UpdateType& textureUpdateType,
                const UpdateType& meshUpdateType):
        m_mesh(nullptr),
        m_texture(nullptr),
        m_textureIndex(UINT32_MAX),
        m_entityIndex(EntityChangeTracker::INVALID_INDEX),
        m_transformUpdateType(transformUpdateType),
        m_textureUpdateType(textureUpdateType),
        m_meshUpdateType(meshUpdateType) {
}

RenderComponent& RenderComponent::setMesh(const std::shared_ptr<Mesh>& mesh) {
    if (mesh.get() != m_mesh.get()) {
        Application::instance()->renderer()->notifyMeshChanged(m_meshUpdateType);
        m_mesh = mesh;
    }
    return *this;
}

RenderComponent& RenderComponent::setTexture(const std::shared_ptr<Texture2D>& texture) {
    uint32_t textureIndex = Application::instance()->renderer()->registerTexture(texture.get());
    if (textureIndex != m_textureIndex) {
        Application::instance()->renderer()->notifyTextureChanged(m_entityIndex);
        m_textureIndex = textureIndex;
        m_texture = texture;
    }

    return *this;
}

//RenderComponent &RenderComponent::setTransformUpdateType(const UpdateType& updateType) {
//    if (updateType != m_transformUpdateType) {
//        transformUpdateTypeChangeTracker().setChanged(m_entityIndex, true);
//        m_transformUpdateType = updateType;
//    }
//    return *this;
//}
//
//RenderComponent &RenderComponent::setTextureUpdateType(const UpdateType& updateType) {
//    if (updateType != m_textureUpdateType) {
//        textureUpdateTypeChangeTracker().setChanged(m_entityIndex, true);
//        m_textureUpdateType = updateType;
//    }
//    return *this;
//}

const std::shared_ptr<Mesh>& RenderComponent::mesh() const {
    return m_mesh;
}

const std::shared_ptr<Texture2D>& RenderComponent::texture() const {
    return m_texture;
}

RenderComponent::UpdateType RenderComponent::transformUpdateType() const {
    return m_transformUpdateType;
}

RenderComponent::UpdateType RenderComponent::textureUpdateType() const {
    return m_textureUpdateType;
}

RenderComponent::UpdateType RenderComponent::meshUpdateType() const {
    return m_meshUpdateType;
}

void RenderComponent::reindex(RenderComponent& renderComponent, const size_t& newEntityIndex) {
    if (newEntityIndex == renderComponent.m_entityIndex)
        return;
    renderComponent.m_entityIndex = newEntityIndex;
}

const uint32_t& RenderComponent::getTextureIndex() const {
    return m_textureIndex;
}
