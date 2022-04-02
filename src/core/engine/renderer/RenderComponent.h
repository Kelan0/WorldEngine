
#ifndef WORLDENGINE_RENDERCOMPONENT_H
#define WORLDENGINE_RENDERCOMPONENT_H

#include "core/core.h"
#include "core/util/EntityChangeTracker.h"

class Texture2D;
class Mesh;

class RenderComponent {
private:
    static EntityChangeTracker s_textureChangeTracker;

public:
    RenderComponent();

    RenderComponent& setMesh(const std::shared_ptr<Mesh>& mesh);

    RenderComponent& setTexture(const std::shared_ptr<Texture2D>& texture);

    const uint32_t& getTextureIndex() const;

    const std::shared_ptr<Mesh>& mesh() const;

    const std::shared_ptr<Texture2D>& texture() const;

    static EntityChangeTracker& textureChangeTracker();

    static void reindex(RenderComponent& renderComponent, const size_t& newEntityIndex);

private:
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture2D> m_texture;

    EntityChangeTracker::entity_index m_entityIndex;
    uint32_t m_textureIndex;
};


#endif //WORLDENGINE_RENDERCOMPONENT_H
