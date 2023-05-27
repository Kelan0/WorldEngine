#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/application/Engine.h"

Frustum::Frustum() :
        Frustum(glm::dvec3(0.0), glm::dmat4(1.0)) {
}

Frustum::Frustum(const RenderCamera& renderCamera) {
    this->set(renderCamera);
}

Frustum::Frustum(const glm::dvec3& origin, const glm::dmat4& viewProjection) {
    this->set(origin, viewProjection);
}

Frustum::Frustum(const Transform& transform, const Camera& camera) {
    this->set(transform, camera);
}

Frustum& Frustum::set(const glm::dvec3& origin, const glm::dmat4& viewProjection) {
    m_origin = origin;

    m_planes[Plane_Left][0] = viewProjection[0][3] + viewProjection[0][0];
    m_planes[Plane_Left][1] = viewProjection[1][3] + viewProjection[1][0];
    m_planes[Plane_Left][2] = viewProjection[2][3] + viewProjection[2][0];
    m_planes[Plane_Left][3] = viewProjection[3][3] + viewProjection[3][0];

    m_planes[Plane_Right][0] = viewProjection[0][3] - viewProjection[0][0];
    m_planes[Plane_Right][1] = viewProjection[1][3] - viewProjection[1][0];
    m_planes[Plane_Right][2] = viewProjection[2][3] - viewProjection[2][0];
    m_planes[Plane_Right][3] = viewProjection[3][3] - viewProjection[3][0];

    m_planes[Plane_Bottom][0] = viewProjection[0][3] + viewProjection[0][1];
    m_planes[Plane_Bottom][1] = viewProjection[1][3] + viewProjection[1][1];
    m_planes[Plane_Bottom][2] = viewProjection[2][3] + viewProjection[2][1];
    m_planes[Plane_Bottom][3] = viewProjection[3][3] + viewProjection[3][1];

    m_planes[Plane_Top][0] = viewProjection[0][3] - viewProjection[0][1];
    m_planes[Plane_Top][1] = viewProjection[1][3] - viewProjection[1][1];
    m_planes[Plane_Top][2] = viewProjection[2][3] - viewProjection[2][1];
    m_planes[Plane_Top][3] = viewProjection[3][3] - viewProjection[3][1];

    m_planes[Plane_Near][0] = viewProjection[0][3] + viewProjection[0][2];
    m_planes[Plane_Near][1] = viewProjection[1][3] + viewProjection[1][2];
    m_planes[Plane_Near][2] = viewProjection[2][3] + viewProjection[2][2];
    m_planes[Plane_Near][3] = viewProjection[3][3] + viewProjection[3][2];

    m_planes[Plane_Far][0] = viewProjection[0][3] - viewProjection[0][2];
    m_planes[Plane_Far][1] = viewProjection[1][3] - viewProjection[1][2];
    m_planes[Plane_Far][2] = viewProjection[2][3] - viewProjection[2][2];
    m_planes[Plane_Far][3] = viewProjection[3][3] - viewProjection[3][2];

    for (size_t i = 0; i < NumPlanes; ++i)
        m_planes[i].normalize();

    for (size_t i = 0; i < NumCorners; ++i)
        m_corners[i].x = NAN; // Invalidate corners. Recalculated whenever getCorner is called.

    return *this;
}

Frustum& Frustum::set(const RenderCamera& renderCamera) {
    this->set(renderCamera.getTransform().getTranslation(), renderCamera.getViewProjectionMatrix());
    return *this;
}

Frustum& Frustum::set(const Transform& transform, const Camera& camera) {
    glm::dmat4 projectionMatrix = camera.getProjectionMatrix();
    glm::dmat4 viewMatrix = glm::inverse(transform.getMatrix());
    glm::dmat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    this->set(transform.getTranslation(), viewProjectionMatrix);
    return *this;
}

const glm::dvec3& Frustum::getOrigin() const {
    return m_origin;
}

const Plane& Frustum::getPlane(size_t planeIndex) const {
    assert(planeIndex < NumPlanes);

    return m_planes[planeIndex];
}

const glm::dvec3& Frustum::getCorner(size_t cornerIndex) const {
    assert(cornerIndex < NumCorners);

    if (glm::isnan(m_corners[cornerIndex].x)) {
        switch (cornerIndex) {
            // m_corners is mutable so that it can be edited from const methods
            case Corner_Left_Top_Near:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Top], m_planes[Plane_Near]);
                break;
            case Corner_Right_Top_Near:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Top], m_planes[Plane_Near]);
                break;
            case Corner_Right_Bottom_Near:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Bottom], m_planes[Plane_Near]);
                break;
            case Corner_Left_Bottom_Near:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Bottom], m_planes[Plane_Near]);
                break;
            case Corner_Left_Top_Far:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Top], m_planes[Plane_Far]);
                break;
            case Corner_Right_Top_Far:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Top], m_planes[Plane_Far]);
                break;
            case Corner_Right_Bottom_Far:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Right], m_planes[Plane_Bottom], m_planes[Plane_Far]);
                break;
            case Corner_Left_Bottom_Far:
                m_corners[cornerIndex] = Plane::triplePlaneIntersection(m_planes[Plane_Left], m_planes[Plane_Bottom], m_planes[Plane_Far]);
                break;
            default:
                break;
        }
    }
    return m_corners[cornerIndex];
}

std::array<glm::dvec3, Frustum::NumCorners> Frustum::getCornersNDC() {
    std::array<glm::dvec3, Frustum::NumCorners> corners{};
    corners[Corner_Left_Top_Near] = glm::dvec3(-1, +1, -1);
    corners[Corner_Right_Top_Near] = glm::dvec3(+1, +1, -1);
    corners[Corner_Right_Bottom_Near] = glm::dvec3(+1, -1, -1);
    corners[Corner_Left_Bottom_Near] = glm::dvec3(-1, -1, -1);
    corners[Corner_Left_Top_Far] = glm::dvec3(-1, +1, +1);
    corners[Corner_Right_Top_Far] = glm::dvec3(+1, +1, +1);
    corners[Corner_Right_Bottom_Far] = glm::dvec3(+1, -1, +1);
    corners[Corner_Left_Bottom_Far] = glm::dvec3(-1, -1, +1);
    return corners;
}


void Frustum::drawLines() const {
    auto* renderer = Engine::instance()->getImmediateRenderer();
    auto createLine = [&renderer](const glm::dvec3& a, const glm::dvec3& b) {
        renderer->vertex(a); // 0
        renderer->vertex(b); // 1
    };

    glm::dvec3 corners[8];
    getRenderCorners(corners);

    renderer->begin(PrimitiveType_Line);

    createLine(corners[Corner_Left_Top_Near], corners[Corner_Left_Top_Far]);
    createLine(corners[Corner_Right_Top_Near], corners[Corner_Right_Top_Far]);
    createLine(corners[Corner_Right_Bottom_Near], corners[Corner_Right_Bottom_Far]);
    createLine(corners[Corner_Left_Bottom_Near], corners[Corner_Left_Bottom_Far]);

    createLine(corners[Corner_Left_Top_Near], corners[Corner_Right_Top_Near]);
    createLine(corners[Corner_Right_Top_Near], corners[Corner_Right_Bottom_Near]);
    createLine(corners[Corner_Right_Bottom_Near], corners[Corner_Left_Bottom_Near]);
    createLine(corners[Corner_Left_Bottom_Near], corners[Corner_Left_Top_Near]);

    createLine(corners[Corner_Left_Top_Far], corners[Corner_Right_Top_Far]);
    createLine(corners[Corner_Right_Top_Far], corners[Corner_Right_Bottom_Far]);
    createLine(corners[Corner_Right_Bottom_Far], corners[Corner_Left_Bottom_Far]);
    createLine(corners[Corner_Left_Bottom_Far], corners[Corner_Left_Top_Far]);

    renderer->end();
}

void Frustum::drawFill() const {
    auto* renderer = Engine::instance()->getImmediateRenderer();
    auto createQuad = [&renderer](const glm::dvec3 normal, const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& c, const glm::dvec3& d) {
        renderer->normal(normal);
        renderer->vertex(a); // 0
        renderer->vertex(b); // 1
        renderer->vertex(c); // 2
        renderer->vertex(a); // 0
        renderer->vertex(c); // 2
        renderer->vertex(d); // 3
    };

    glm::dvec3 corners[8];
    getRenderCorners(corners);

    renderer->begin(PrimitiveType_Triangle);
    createQuad(getPlane(Plane_Left).getNormal(), corners[Corner_Left_Top_Near], corners[Corner_Left_Top_Far], corners[Corner_Left_Bottom_Far], corners[Corner_Left_Bottom_Near]);
    createQuad(getPlane(Plane_Right).getNormal(), corners[Corner_Right_Top_Far], corners[Corner_Right_Top_Near], corners[Corner_Right_Bottom_Near], corners[Corner_Right_Bottom_Far]);
    createQuad(getPlane(Plane_Bottom).getNormal(), corners[Corner_Left_Top_Near], corners[Corner_Right_Top_Near], corners[Corner_Right_Top_Far], corners[Corner_Left_Top_Far]);
    createQuad(getPlane(Plane_Top).getNormal(), corners[Corner_Right_Bottom_Near], corners[Corner_Left_Bottom_Near], corners[Corner_Left_Bottom_Far], corners[Corner_Right_Bottom_Far]);
//    createQuad(getPlane(Plane_Near).getNormal(), corners[Corner_Right_Top_Near], corners[Corner_Left_Top_Near], corners[Corner_Left_Bottom_Near], corners[Corner_Right_Bottom_Near]);
//    createQuad(getPlane(Plane_Far).getNormal(), corners[Corner_Left_Top_Far], corners[Corner_Right_Top_Far], corners[Corner_Right_Bottom_Far], corners[Corner_Left_Bottom_Far]);
    renderer->end();
}

void Frustum::getRenderCorners(glm::dvec3* corners) const {
    corners[Corner_Left_Top_Near] = getCorner(Corner_Left_Top_Near);
    corners[Corner_Right_Top_Near] = getCorner(Corner_Right_Top_Near);
    corners[Corner_Right_Bottom_Near] = getCorner(Corner_Right_Bottom_Near);
    corners[Corner_Left_Bottom_Near] = getCorner(Corner_Left_Bottom_Near);
    corners[Corner_Left_Top_Far] = getCorner(Corner_Left_Top_Far);
    corners[Corner_Right_Top_Far] = getCorner(Corner_Right_Top_Far);
    corners[Corner_Right_Bottom_Far] = getCorner(Corner_Right_Bottom_Far);
    corners[Corner_Left_Bottom_Far] = getCorner(Corner_Left_Bottom_Far);

    glm::dvec3 vTL = corners[Corner_Left_Top_Far] - corners[Corner_Left_Top_Near];
    glm::dvec3 vTR = corners[Corner_Right_Top_Far] - corners[Corner_Right_Top_Near];
    glm::dvec3 vBR = corners[Corner_Right_Bottom_Far] - corners[Corner_Right_Bottom_Near];
    glm::dvec3 vBL = corners[Corner_Left_Bottom_Far] - corners[Corner_Left_Bottom_Near];

    corners[Corner_Left_Top_Far] = corners[Corner_Left_Top_Near] + vTL * 0.75;
    corners[Corner_Right_Top_Far] = corners[Corner_Right_Top_Near] + vTR * 0.75;
    corners[Corner_Right_Bottom_Far] = corners[Corner_Right_Bottom_Near] + vBR * 0.75;
    corners[Corner_Left_Bottom_Far] = corners[Corner_Left_Bottom_Near] + vBL * 0.75;
}

bool Frustum::intersects(const BoundingVolume& boundingVolume) const {
    for (size_t i = 0; i < NumPlanes; ++i)
        if (m_planes[i].calculateMaxSignedDistance(boundingVolume) < 0.0F)
            return false; // The most positive coordinate of this bounding volume is on the negative side of the plane (volume fully outside frustum)
    return true;
}

bool Frustum::contains(const BoundingVolume& boundingVolume) const {
    for (size_t i = 0; i < NumPlanes; ++i)
        if (m_planes[i].calculateMinSignedDistance(boundingVolume) < 0.0F)
            return false; // The most negative coordinate of this bounding volume is on the negative side of the plane (volume at least partially outside frustum, not fully contained)
    return true;
}

bool Frustum::contains(const glm::dvec3& point) const {
    for (size_t i = 0; i < NumPlanes; ++i) {
        if (m_planes[i].calculateSignedDistance(point) < 0.0)
            return false;
    }
    return true; // Point is on the positive side of all 6 frustum planes.
}

