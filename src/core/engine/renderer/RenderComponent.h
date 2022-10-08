
#ifndef WORLDENGINE_RENDERCOMPONENT_H
#define WORLDENGINE_RENDERCOMPONENT_H

#include "core/core.h"
#include "core/util/EntityChangeTracker.h"

class Material;
class Mesh;


class RenderComponent {
    friend class SceneRenderer;
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

    RenderComponent(const UpdateType& transformUpdateType,
                    const UpdateType& materialUpdateType,
                    const UpdateType& meshUpdateType);

    RenderComponent& setMesh(const std::shared_ptr<Mesh>& mesh);

    RenderComponent& setMaterial(const std::shared_ptr<Material>& material);

    [[nodiscard]] const uint32_t& getMaterialIndex() const;

    [[nodiscard]] const std::shared_ptr<Mesh>& mesh() const;

    [[nodiscard]] const std::shared_ptr<Material>& material() const;

    [[nodiscard]] UpdateType transformUpdateType() const;

    [[nodiscard]] UpdateType materialUpdateType() const;

    [[nodiscard]] UpdateType meshUpdateType() const;

private:
    static void reindex(RenderComponent& renderComponent, const EntityChangeTracker::entity_index& newEntityIndex);

private:
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Material> m_material;

    EntityChangeTracker::entity_index m_entityIndex;
    uint32_t m_materialIndex;

    struct {
        UpdateType m_transformUpdateType : 2;
        UpdateType m_materialUpdateType : 2;
        UpdateType m_meshUpdateType : 2;
    };
};


#endif //WORLDENGINE_RENDERCOMPONENT_H
