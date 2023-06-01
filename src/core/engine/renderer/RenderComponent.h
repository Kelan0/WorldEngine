
#ifndef WORLDENGINE_RENDERCOMPONENT_H
#define WORLDENGINE_RENDERCOMPONENT_H

#include "core/core.h"

class BoundingVolume;
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

    RenderComponent(const UpdateType& transformUpdateType, const UpdateType& meshUpdateType);

    RenderComponent& setMesh(const std::shared_ptr<Mesh>& mesh);

    RenderComponent& setMaterial(const std::shared_ptr<Material>& material);

    RenderComponent& setBoundingVolume(BoundingVolume* boundingVolume);

    const std::shared_ptr<Mesh>& getMesh() const;

    const std::shared_ptr<Material>& getMaterial() const;

    BoundingVolume* getBoundingVolume() const;

    UpdateType transformUpdateType() const;

    UpdateType meshUpdateType() const;

private:
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Material> m_material;
    BoundingVolume* m_boundingVolume;

    struct {
        UpdateType m_transformUpdateType : 2;
        UpdateType m_meshUpdateType : 2;
    };
};


#endif //WORLDENGINE_RENDERCOMPONENT_H
