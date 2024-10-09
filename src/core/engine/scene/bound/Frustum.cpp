#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/bound/BoundingVolume.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/RenderCamera.h"
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

Frustum::Frustum(const Frustum& frustum) :
        m_origin(frustum.m_origin),
        m_planes(frustum.m_planes),
        m_corners(frustum.m_corners),
        m_camera(frustum.m_camera) {
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

    m_camera.m_near = NAN;

    return *this;
}

Frustum& Frustum::set(const RenderCamera& renderCamera) {
//    this->set(renderCamera.getTransform().getTranslation(), renderCamera.getViewProjectionMatrix());
    this->set(renderCamera.getTransform(), renderCamera.getProjection());
    return *this;
}

Frustum& Frustum::set(const Transform& transform, const Camera& camera) {
    glm::dmat4 projectionMatrix = camera.getProjectionMatrix();
    glm::dmat4 viewMatrix = glm::inverse(transform.getMatrix());
    glm::dmat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
    this->set(transform.getTranslation(), viewProjectionMatrix);
    m_camera = camera;
    return *this;
}

const glm::dvec3& Frustum::getOrigin() const {
    return m_origin;
}

const glm::dvec3& Frustum::getForwardAxis() const {
    return m_planes[Plane_Near].getNormal();
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

const std::array<glm::dvec3, Frustum::NumCorners>& Frustum::getCorners() const {
    for (int i = 0; i < NumCorners; ++i) {
        getCorner(i);
    }
    return m_corners;
}

Camera Frustum::getCamera() const {
    if (glm::isnan(m_camera.getNear())) {
        const std::array<glm::dvec3, NumCorners> corners = getCorners();
        double near = m_planes[Plane_Near].calculateSignedDistance(m_origin);
        double far = m_planes[Plane_Far].calculateSignedDistance(m_origin);
        glm::dvec3 center = m_planes[Plane_Near].normal * near;
        double top = glm::distance(center, (corners[Corner_Left_Top_Near] + corners[Corner_Right_Top_Near]) * 0.5);
        double bottom = glm::distance(center, (corners[Corner_Left_Bottom_Near] + corners[Corner_Right_Bottom_Near]) * 0.5);
        double left = glm::distance(center, (corners[Corner_Left_Top_Near] + corners[Corner_Left_Bottom_Near]) * 0.5);
        double right = glm::distance(center, (corners[Corner_Right_Top_Near] + corners[Corner_Right_Bottom_Near]) * 0.5);
        m_camera = Camera(left, right, bottom, top, near, far, isOrtho());
    }
    return m_camera;
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

Visibility Frustum::intersectsAABB(const AxisAlignedBoundingBox& boundingBox) const {
    // https://iquilezles.org/articles/frustumcorrect/

    glm::dvec4 plane;

    int in = 0;
    // check box outside/inside of frustum
    for (int i = 0; i < NumPlanes; ++i) {
        int out = 0;
        plane = (glm::dvec4)m_planes[i];
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMinX(), boundingBox.getBoundMinY(), boundingBox.getBoundMinZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMaxX(), boundingBox.getBoundMinY(), boundingBox.getBoundMinZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMinX(), boundingBox.getBoundMaxY(), boundingBox.getBoundMinZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMaxX(), boundingBox.getBoundMaxY(), boundingBox.getBoundMinZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMinX(), boundingBox.getBoundMinY(), boundingBox.getBoundMaxZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMaxX(), boundingBox.getBoundMinY(), boundingBox.getBoundMaxZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMinX(), boundingBox.getBoundMaxY(), boundingBox.getBoundMaxZ(), 1.0)) < 0.0) ? 1 : 0);
        out += ((dot(plane, glm::dvec4(boundingBox.getBoundMaxX(), boundingBox.getBoundMaxY(), boundingBox.getBoundMaxZ(), 1.0)) < 0.0) ? 1 : 0);
        if (out == 8) return Visibility_NotVisible; // All corners of the AABB are outside this frustum plane. The AABB is not visible.
        if (out == 0) ++in; // The AABB was fully inside this frustum plane.
    }

    if (in == NumPlanes) {
        return Visibility_FullyVisible; // The AABB was fully inside all checked frustum planes. It is fully visible.
    }

    // check frustum outside/inside box
    const std::array<glm::dvec3, NumCorners>& corners = getCorners();
    int i, out;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].x > boundingBox.getBoundMaxX()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].x < boundingBox.getBoundMinX()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].y > boundingBox.getBoundMaxY()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].y < boundingBox.getBoundMinY()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].z > boundingBox.getBoundMaxZ()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;
    for (i = 0, out = 0; i < NumCorners; ++i) out += ((corners[i].z < boundingBox.getBoundMinZ()) ? 1 : 0);
    if (out == 8) return Visibility_NotVisible;

    return Visibility_PartiallyVisible;
}

bool Frustum::intersects(const BoundingVolume& boundingVolume) const {
    for (const auto& m_plane : m_planes)
        if (m_plane.calculateMaxSignedDistance(boundingVolume) < 0.0F)
            return false; // The most positive coordinate of this bounding volume is on the negative side of the plane (volume fully outside frustum)
    return true;
}

bool Frustum::contains(const BoundingVolume& boundingVolume) const {
    for (const auto& m_plane : m_planes)
        if (m_plane.calculateMinSignedDistance(boundingVolume) < 0.0F)
            return false; // The most negative coordinate of this bounding volume is on the negative side of the plane (volume at least partially outside frustum, not fully contained)
    return true;
}

bool Frustum::contains(const glm::dvec3& point) const {
    for (const auto& m_plane : m_planes) {
        if (m_plane.calculateSignedDistance(point) < 0.0)
            return false;
    }
    return true; // Point is on the positive side of all 6 frustum planes.
}

bool Frustum::isOrtho() const {
    return Plane::isParallel(m_planes[Plane_Top], m_planes[Plane_Bottom]);
}

double Frustum::calculateVerticalFov() const {
    return Plane::angle(m_planes[Plane_Top], m_planes[Plane_Bottom]);
}

double Frustum::calculateProjectedSize(double radius, double distance) const {
    double top = m_planes[Plane_Top].offset;
    double bottom = m_planes[Plane_Bottom].offset;

    if (isOrtho()) {
        return Camera::calculateProjectedOrthographicSize(radius, top, bottom);
    } else {
        return Camera::calculateProjectedPerspectiveSize(radius, distance, calculateVerticalFov(), top, bottom);
    }
}

Frustum& Frustum::operator=(const Frustum& copy) {
    if (this == &copy) {
        return *this;
    }
    m_origin = copy.m_origin;
    m_planes = copy.m_planes;
    m_corners = copy.m_corners;
    return *this;
}

bool Frustum::operator==(const Frustum& frustum) const {
    for (int i = 0; i < NumPlanes; ++i) {
        if (m_planes[i] != frustum.m_planes[i]) {
            return false;
        }
    }
    return true;
}

bool Frustum::operator!=(const Frustum& frustum) const {
    return !(*this == frustum);
}

Frustum Frustum::transform(const Frustum& frustum, const glm::dmat4& matrix) {
    Frustum returnValue{};

    returnValue.m_origin = glm::dvec3(matrix * glm::dvec4(frustum.getOrigin(), 1.0));

    for (int i = 0; i < NumPlanes; ++i) {
        returnValue.m_planes[i] = Plane::transform(frustum.getPlane(i), matrix);
    }
    return returnValue;
}

