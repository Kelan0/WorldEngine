#include "core/engine/renderer/RenderComponent.h"
#include "core/application/Application.h"
#include "core/engine/renderer/SceneRenderer.h"

EntityChangeTracker RenderComponent::s_textureChangeTracker;

RenderComponent::RenderComponent():
    m_mesh(nullptr),
    m_texture(nullptr),
    m_textureIndex(UINT32_MAX),
    m_entityIndex(EntityChangeTracker::INVALID_INDEX) {
}

RenderComponent& RenderComponent::setMesh(const std::shared_ptr<Mesh>& mesh) {
    m_mesh = mesh;
    return *this;
}

RenderComponent& RenderComponent::setTexture(const std::shared_ptr<Texture2D>& texture) {
    uint32_t textureIndex = Application::instance()->renderer()->registerTexture(texture.get());
    if (textureIndex != m_textureIndex) {
        textureChangeTracker().setChanged(m_entityIndex, true);
        m_textureIndex = textureIndex;
    }

    m_texture = texture;
    return *this;
}

const std::shared_ptr<Mesh>& RenderComponent::mesh() const {
    return m_mesh;
}

const std::shared_ptr<Texture2D>& RenderComponent::texture() const {
    return m_texture;
}

EntityChangeTracker& RenderComponent::textureChangeTracker() {
    return s_textureChangeTracker;
}

void RenderComponent::reindex(RenderComponent& renderComponent, const size_t& newEntityIndex) {
    s_textureChangeTracker.reindex(renderComponent.m_entityIndex, newEntityIndex);
}

const uint32_t& RenderComponent::getTextureIndex() const {
    return m_textureIndex;
}
