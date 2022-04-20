//
// Created by Kelan on 19/04/2022.
//

#include "Frustum.h"

Frustum::Frustum():
    Frustum(glm::dmat4(1.0)) {
}

Frustum::Frustum(const RenderCamera& renderCamera) {
    this->set(renderCamera);
}

Frustum::Frustum(const glm::dmat4& viewProjection) {
    this->set(viewProjection);
}

Frustum::Frustum(const Transform& transform, const Camera& camera) {
    this->set(transform, camera);
}

Frustum &Frustum::set(const glm::dmat4& viewProjection) {

    m_planes[Plane_Left].a = viewProjection[0][3] + viewProjection[0][0];
    m_planes[Plane_Left].b = viewProjection[1][3] + viewProjection[1][0];
    m_planes[Plane_Left].c = viewProjection[2][3] + viewProjection[2][0];
    m_planes[Plane_Left].d = viewProjection[3][3] + viewProjection[3][0];

    m_planes[Plane_Right].a = viewProjection[0][3] - viewProjection[0][0];
    m_planes[Plane_Right].b = viewProjection[1][3] - viewProjection[1][0];
    m_planes[Plane_Right].c = viewProjection[2][3] - viewProjection[2][0];
    m_planes[Plane_Right].d = viewProjection[3][3] - viewProjection[3][0];

    m_planes[Plane_Bottom].a = viewProjection[0][3] + viewProjection[0][1];
    m_planes[Plane_Bottom].b = viewProjection[1][3] + viewProjection[1][1];
    m_planes[Plane_Bottom].c = viewProjection[2][3] + viewProjection[2][1];
    m_planes[Plane_Bottom].d = viewProjection[3][3] + viewProjection[3][1];

    m_planes[Plane_Top].a = viewProjection[0][3] - viewProjection[0][1];
    m_planes[Plane_Top].b = viewProjection[1][3] - viewProjection[1][1];
    m_planes[Plane_Top].c = viewProjection[2][3] - viewProjection[2][1];
    m_planes[Plane_Top].d = viewProjection[3][3] - viewProjection[3][1];

    m_planes[Plane_Near].a = viewProjection[0][3] + viewProjection[0][2];
    m_planes[Plane_Near].b = viewProjection[1][3] + viewProjection[1][2];
    m_planes[Plane_Near].c = viewProjection[2][3] + viewProjection[2][2];
    m_planes[Plane_Near].d = viewProjection[3][3] + viewProjection[3][2];

    m_planes[Plane_Far].a = viewProjection[0][3] - viewProjection[0][2];
    m_planes[Plane_Far].b = viewProjection[1][3] - viewProjection[1][2];
    m_planes[Plane_Far].c = viewProjection[2][3] - viewProjection[2][2];
    m_planes[Plane_Far].d = viewProjection[3][3] - viewProjection[3][2];

    for (size_t i = 0; i < NumPlanes; ++i)
        m_planes[i].normalize();

    for (size_t i = 0; i < NumCorners; ++i)
        m_corners[i].x = NAN; // Invalidate corners. Recalculated whenever getCorner is called.

    return *this;
}

Frustum &Frustum::set(const RenderCamera& renderCamera) {
    this->set(renderCamera.getViewProjectionMatrix());
    return *this;
}

Frustum &Frustum::set(const Transform& transform, const Camera& camera) {
    glm::dmat4 projectionMatrix = camera.getProjectionMatrix();
    glm::dmat4 viewMatrix = glm::inverse(transform.getMatrix());
    glm::dmat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    this->set(viewProjectionMatrix);
    return *this;
}

bool Frustum::containsPoint(const glm::dvec3& point) {
    return containsPoint(point.x, point.y, point.z);
}

bool Frustum::containsPoint(const double& x, const double& y, const double& z) {
    for (size_t i = 0; i < 6; ++i) {
        if (m_planes[i].signedDistance(x, y, z) < 0.0)
            return false;
    }
    return true; // Point is on the positive side of all 6 frustum planes.
}

const Plane& Frustum::getPlane(const uint32_t& planeIndex) const {
    assert(planeIndex < NumPlanes);

    return m_planes[planeIndex];
}

const glm::dvec3& Frustum::getCorner(const uint32_t& cornerIndex) const {
    assert(cornerIndex < NumCorners);

    if (glm::isnan(m_corners[cornerIndex].x)) {
        switch (cornerIndex) {
            // m_corners is mutable so that it can be edited from const methods
            case Corner_Left_Top_Near: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Top], m_planes[Plane_Near]); break;
            case Corner_Right_Top_Near: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Top], m_planes[Plane_Near]); break;
            case Corner_Right_Bottom_Near: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Bottom], m_planes[Plane_Near]); break;
            case Corner_Left_Bottom_Near: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Bottom], m_planes[Plane_Near]); break;
            case Corner_Left_Top_Far: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Top], m_planes[Plane_Far]); break;
            case Corner_Right_Top_Far: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Top], m_planes[Plane_Far]); break;
            case Corner_Right_Bottom_Far: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Bottom], m_planes[Plane_Far]); break;
            case Corner_Left_Bottom_Far: m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Bottom], m_planes[Plane_Far]); break;
        }
    }
    return m_corners[cornerIndex];
}

