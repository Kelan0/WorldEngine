
#ifndef WORLDENGINE_RENDERCOMPONENT_H
#define WORLDENGINE_RENDERCOMPONENT_H

#include "core/core.h"
#include "core/util/EntityChangeTracker.h"

class Texture2D;
class Mesh;


class RenderComponent {
public:
    enum UpdateType {
        // The entity is never changed. Doing so incurs a significant performance penalty.
        // Useful for static level geometry.
        UpdateType_Static = 0,

        // The entity can change frequently, and is re-uploaded whenever a change is detected.
        UpdateType_Dynamic = 1,

        // The entity is expected to change every frame, so its data always gets re-uploaded
        // without checking if it changed.
        UpdateType_Always = 3,
    };

public:
    RenderComponent();

    RenderComponent& setMesh(const std::shared_ptr<Mesh>& mesh);

    RenderComponent& setTexture(const std::shared_ptr<Texture2D>& texture);

    const uint32_t& getTextureIndex() const;

    const std::shared_ptr<Mesh>& mesh() const;

    const std::shared_ptr<Texture2D>& texture() const;

    UpdateType transformUpdateType() const;

    UpdateType textureUpdateType() const;

    UpdateType meshUpdateType() const;

    static void reindex(RenderComponent& renderComponent, const size_t& newEntityIndex);

private:
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture2D> m_texture;

    EntityChangeTracker::entity_index m_entityIndex;
    uint32_t m_textureIndex;

    struct {
        UpdateType m_transformUpdateType : 2;
        UpdateType m_textureUpdateType : 2;
        UpdateType m_meshUpdateType : 2;
    };
};


#endif //WORLDENGINE_RENDERCOMPONENT_H
