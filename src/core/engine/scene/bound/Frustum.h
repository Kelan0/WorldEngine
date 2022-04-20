#ifndef WORLDENGINE_FRUSTUM_H
#define WORLDENGINE_FRUSTUM_H

#include "core/core.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/scene/bound/Plane.h"

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

    Frustum(const glm::dmat4& viewProjection);

    Frustum(const RenderCamera& renderCamera);

    Frustum(const Transform& transform, const Camera& camera);

    Frustum& set(const glm::dmat4& viewProjection);

    Frustum& set(const RenderCamera& renderCamera);

    Frustum& set(const Transform& transform, const Camera& camera);

    bool containsPoint(const glm::dvec3& point);

    bool containsPoint(const double& x, const double& y, const double& z);

    const Plane& getPlane(const uint32_t& planeIndex) const;

    const glm::dvec3& getCorner(const uint32_t& cornerIndex) const;

private:
    Plane m_planes[NumPlanes];
    mutable glm::dvec3 m_corners[NumCorners];
};


#endif //WORLDENGINE_FRUSTUM_H
