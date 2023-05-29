#ifndef WORLDENGINE_FRUSTUM_H
#define WORLDENGINE_FRUSTUM_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/scene/bound/Plane.h"
#include "core/engine/scene/bound/BoundingVolume.h"

class Frustum {
public:
    enum {
        Plane_Left = 0,
        Plane_Right = 1,
        Plane_Bottom = 2,
        Plane_Top = 3,
        Plane_Near = 4,
        Plane_Far = 5,
        NumPlanes = 6,
    };

    enum {
        Corner_Left_Top_Near = 0,
        Corner_Right_Top_Near = 1,
        Corner_Right_Bottom_Near = 2,
        Corner_Left_Bottom_Near = 3,
        Corner_Left_Top_Far = 4,
        Corner_Right_Top_Far = 5,
        Corner_Right_Bottom_Far = 6,
        Corner_Left_Bottom_Far = 7,
        NumCorners = 8,
    };

public:
    Frustum();

    explicit Frustum(const glm::dvec3& origin, const glm::dmat4& viewProjection);

    explicit Frustum(const RenderCamera& renderCamera);

    Frustum(const Transform& transform, const Camera& camera);

    Frustum& set(const glm::dvec3& origin, const glm::dmat4& viewProjection);

    Frustum& set(const RenderCamera& renderCamera);

    Frustum& set(const Transform& transform, const Camera& camera);

    const glm::dvec3& getOrigin() const;

    const glm::dvec3& getForwardAxis() const;

    const Plane& getPlane(size_t planeIndex) const;

    const glm::dvec3& getCorner(size_t cornerIndex) const;

    static std::array<glm::dvec3, NumCorners> getCornersNDC();

    void drawLines() const;

    void drawFill() const;

    bool intersects(const BoundingVolume& boundingVolume) const;

    bool contains(const BoundingVolume& boundingVolume) const;

    bool contains(const glm::dvec3& point) const;

    static Frustum transform(const Frustum& frustum, const glm::dmat4& matrix);

private:
    void getRenderCorners(glm::dvec3* corners) const;

private:
    glm::dvec3 m_origin;
    Plane m_planes[NumPlanes];
    mutable glm::dvec3 m_corners[NumCorners];
};


#endif //WORLDENGINE_FRUSTUM_H
