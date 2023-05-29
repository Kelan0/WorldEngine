#include "core/engine/scene/bound/BoundingVolume.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/application/Engine.h"


// Test if two spheres intersect
bool intersects_Sphere_Sphere(const BoundingSphere& a, const BoundingSphere& b) {
    glm::dvec3 dCenter = a.getCenter() - b.getCenter();
    double distanceSquared = glm::dot(dCenter, dCenter);
    double sumRadius = a.getRadius() + b.getRadius();
    return sumRadius * sumRadius <= distanceSquared;
}

// Test if a sphere and AABB intersect
// Implementation from https://github.com/erich666/GraphicsGems/blob/master/gems/BoxSphere.c (solid box, solid sphere)
bool intersects_Sphere_AABB(const BoundingSphere& a, const AxisAlignedBoundingBox& b) {
    double dmin = 0.0;
    double d;
    glm::dvec3 sphereCenter = a.getCenter();
    glm::dvec3 bMin = b.getBoundMin();
    glm::dvec3 bMax = b.getBoundMax();

    for (int i = 0; i < 3; ++i) {
        if (sphereCenter[i] < bMin[i]) {
            d = sphereCenter[i] - bMin[i];
            dmin += d * d;
        } else if (sphereCenter[i] > bMax[i]) {
            d = sphereCenter[i] - bMax[i];
            dmin += d * d;
        }
    }

    return dmin <= a.getRadius() * a.getRadius();
}

// Test if two AABBs intersect
bool intersects_AABB_AABB(const AxisAlignedBoundingBox& a, const AxisAlignedBoundingBox& b) {
    glm::dvec3 aMin = a.getBoundMin();
    glm::dvec3 aMax = a.getBoundMax();
    glm::dvec3 bMin = b.getBoundMin();
    glm::dvec3 bMax = b.getBoundMax();
    return	(aMin.x <= bMax.x && aMax.x >= bMin.x) &&
              (aMin.y <= bMax.y && aMax.y >= bMin.y) &&
              (aMin.z <= bMax.z && aMax.z >= bMin.z);
}

// Test if sphere A contains point B
bool contains_Sphere_Point(const BoundingSphere& a, const glm::dvec3& b) {
    glm::dvec3 dCenter = b - a.getCenter();
    double distanceSquared = glm::dot(dCenter, dCenter);
    return distanceSquared < a.getRadius() * a.getRadius();
}

// Test if sphere A contains sphere B
bool contains_Sphere_Sphere(const BoundingSphere& a, const BoundingSphere& b) {
    if (b.getRadius() > a.getRadius()) // B is bigger than A, therefor A cannot contain B
        return false;
    glm::dvec3 dCenter = a.getCenter() - b.getCenter();
    double distanceSquared = glm::dot(dCenter, dCenter);
    double maxDistance = a.getRadius() - b.getRadius();
    return distanceSquared < maxDistance * maxDistance;
}

// Test if sphere A contains AABB B
// Implementation from https://github.com/erich666/GraphicsGems/blob/master/gems/BoxSphere.c (hollow sphere, solid box)
bool contains_Sphere_AABB(const BoundingSphere& a, const AxisAlignedBoundingBox& b) {
    glm::dvec3 sphereCenter = a.getCenter();
    double radiusSquared = a.getRadius() * a.getRadius();
    glm::dvec3 bMin = b.getBoundMin();
    glm::dvec3 bMax = b.getBoundMax();
    double dmin = 0.0;
    double dmax = 0.0;
    double d1, d2;
    for (int i = 0; i < 3; ++i) {
        d1 = sphereCenter[i] - bMin[i];
        d1 *= d1;
        d2 = sphereCenter[i] - bMax[i];
        d2 *= d2;
        dmax += glm::max(d1, d2);
        if (sphereCenter[i] < bMin[i]) {
            dmin += d1;
        } else if (sphereCenter[i] > bMax[i]) {
            dmin += d2;
        }
    }
    return dmin <= radiusSquared && radiusSquared <= dmax;
}

// Test if AABB A contains point B
bool contains_AABB_Point(const AxisAlignedBoundingBox& a, const glm::dvec3& b) {
    glm::dvec3 aMin = a.getBoundMin();
    glm::dvec3 aMax = a.getBoundMax();

    return b.x >= aMin.x && b.x <= aMax.x &&
            b.y >= aMin.y && b.y <= aMax.y &&
            b.z >= aMin.z && b.z <= aMax.z;
}
// Test if AABB A contains sphere B
// Implementation from https://github.com/erich666/GraphicsGems/blob/master/gems/BoxSphere.c (hollow box, solid sphere)
bool contains_AABB_Sphere(const AxisAlignedBoundingBox& a, const BoundingSphere& b) {
    glm::dvec3 sphereCenter = b.getCenter();
    double sphereRadius = b.getRadius();
    glm::dvec3 bMin = a.getBoundMin();
    glm::dvec3 bMax = a.getBoundMax();
    double dmin = 0.0;
    double d;
    bool face = false;

    for (int i = 0; i < 3; i++ ) {
        if (sphereCenter[i] < bMin[i] ) {
            face = true;
            d = sphereCenter[i] - bMin[i];
            dmin += d * d;
        } else if (sphereCenter[i] > bMax[i] ) {
            face = true;
            d = sphereCenter[i] - bMax[i];
            dmin += d * d;
        } else if (sphereCenter[i] - bMin[i] <= sphereRadius) {
            face = true;
        } else if (bMax[i] - sphereCenter[i] <= sphereRadius) {
            face = true;
        }
    }
    return face && (dmin <= sphereRadius * sphereRadius);
}

// Test if AABB A contains AABB B
bool contains_AABB_AABB(const AxisAlignedBoundingBox& a, const AxisAlignedBoundingBox& b) {
    if (a.getHalfExtents().x < b.getHalfExtents().x ||
        a.getHalfExtents().y < b.getHalfExtents().y ||
        a.getHalfExtents().z < b.getHalfExtents().z) {
        return false; // AABB A is smaller than AABB B on at least one axis, therefor A cannot contain B
    }

    glm::dvec3 aMin = a.getBoundMin();
    glm::dvec3 aMax = a.getBoundMax();
    glm::dvec3 bMin = b.getBoundMin();
    glm::dvec3 bMax = b.getBoundMax();

    return bMin.x > aMin.x && bMax.x < aMax.x &&
           bMin.y > aMin.y && bMax.y < aMax.y &&
           bMin.z > aMin.z && bMax.z < aMax.z;
}

double calculateMinDistance_Sphere_Point(const BoundingSphere& a, const glm::dvec3& b) {
    return glm::distance(a.getCenter(), b) - (a.getRadius());
}

double calculateMinDistance_Sphere_Sphere(const BoundingSphere& a, const BoundingSphere& b) {
    return glm::distance(a.getCenter(), b.getCenter()) - (a.getRadius() + b.getRadius());
}

double calculateMinDistance_Sphere_AABB(const BoundingSphere& a, const AxisAlignedBoundingBox& b) {
    glm::dvec3 dist = glm::abs(a.getCenter() - b.getCenter()) - b.getHalfExtents();
    return glm::length(dist) - a.getRadius();
}

double calculateMinDistance_AABB_Point(const AxisAlignedBoundingBox& a, const glm::dvec3& b) {
    glm::dvec3 dist = glm::abs(b - a.getCenter()) - a.getHalfExtents();
    return glm::length(dist);
}

double calculateMinDistance_AABB_AABB(const AxisAlignedBoundingBox& a, const AxisAlignedBoundingBox& b) {
    glm::dvec3 aMin = a.getBoundMin();
    glm::dvec3 aMax = a.getBoundMax();
    glm::dvec3 bMin = b.getBoundMin();
    glm::dvec3 bMax = b.getBoundMax();
    glm::dvec3 oMin = glm::max(aMin, bMin);
    glm::dvec3 oMax = glm::min(aMax, bMax);

    double distSq = 0.0;
    double d;
    for (int i = 0; i < 3; ++i) {
        d = oMin[i] - oMax[i];
        distSq += d * d;
    }
    return glm::sqrt(distSq);
}

glm::dvec3 calculateClosestPoint_Sphere_Point(const BoundingSphere& a, const glm::dvec3& b) {
    glm::dvec3 direction = a.getCenter() - b; // vector from B to A
    double distance = glm::length(direction);
    direction /= distance; // Normalize direction
    distance -= a.getRadius(); // Distance to surface, not center
    return b + direction * distance;
}

glm::dvec3 calculateClosestPoint_AABB_Point(const AxisAlignedBoundingBox& a, const glm::dvec3& b) {
    glm::dvec3 result = b;
    glm::dvec3 bMin = a.getBoundMin();
    glm::dvec3 bMax = a.getBoundMax();

    result.x = (result.x < bMin.x) ? bMin.x : result.x;
    result.y = (result.y < bMin.x) ? bMin.y : result.y;
    result.z = (result.z < bMin.x) ? bMin.z : result.z;

    result.x = (result.x > bMax.x) ? bMax.x : result.x;
    result.y = (result.y > bMax.x) ? bMax.y : result.y;
    result.z = (result.z > bMax.x) ? bMax.z : result.z;

    return result;
}





BoundingVolume::BoundingVolume(Type type):
    m_type(type) {
}

BoundingVolume::~BoundingVolume() = default;

BoundingVolume::Type BoundingVolume::getType() const {
    return m_type;
}




BoundingSphere::BoundingSphere(const glm::dvec3& center, double radius):
    BoundingVolume(Type_Sphere),
    m_center(center),
    m_radius(radius) {
}

BoundingSphere::BoundingSphere(double centerX, double centerY, double centerZ, double radius):
    BoundingVolume(Type_Sphere),
    m_center(centerX, centerY, centerZ),
    m_radius(radius) {
}

BoundingSphere::~BoundingSphere() = default;

glm::dvec3 BoundingSphere::getCenter() const {
    return m_center;
}

void BoundingSphere::setCenter(const glm::dvec3& center) {
    m_center = center;
}

void BoundingSphere::setCenter(double centerX, double centerY, double centerZ) {
    m_center.x = centerX;
    m_center.y = centerY;
    m_center.z = centerZ;
}

double BoundingSphere::getRadius() const {
    return m_radius;
}

void BoundingSphere::setRadius(double radius) {
    m_radius = radius;
}

bool BoundingSphere::intersects(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return intersects_Sphere_Sphere(*this, cast<const BoundingSphere&>(other));

        case Type_AxisAlignedBoundingBox:
            return intersects_Sphere_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return false;

    }
}

bool BoundingSphere::contains(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return contains_Sphere_Sphere(*this, cast<const BoundingSphere&>(other));

        case Type_AxisAlignedBoundingBox:
            return contains_Sphere_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return false;
    }
}

bool BoundingSphere::contains(const glm::dvec3& other) const {
    return contains_Sphere_Point(*this, other);
}

double BoundingSphere::calculateMinDistance(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return calculateMinDistance_Sphere_Sphere(*this, cast<const BoundingSphere&>(other));

        case Type_AxisAlignedBoundingBox:
            return calculateMinDistance_Sphere_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return 0.0;
    }
}

double BoundingSphere::calculateMinDistance(const glm::dvec3& other) const {
    return calculateMinDistance_Sphere_Point(*this, other);
}

glm::dvec3 BoundingSphere::calculateClosestPoint(const glm::dvec3& point) const {
    return calculateClosestPoint_Sphere_Point(*this, point);
}

void BoundingSphere::drawLines() const {
    static MeshData<Vertex> disc(PrimitiveType_Line);

    if (disc.getVertexCount() == 0) {
        const uint32_t discSegments = 20;
        disc.createDisc(glm::vec3(0.0F), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), 1.0F, discSegments);
        disc.pushTransform();
        disc.rotateDegrees(90.0F, 1.0F, 0.0F, 0.0F);
        disc.createDisc(glm::vec3(0.0F), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), 1.0F, discSegments);
        disc.popTransform();
        disc.pushTransform();
        disc.rotateDegrees(90.0F, 0.0F, 1.0F, 0.0F);
        disc.createDisc(glm::vec3(0.0F), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), 1.0F, discSegments);
        disc.popTransform();
    }

    ImmediateRenderer* renderer = Engine::instance()->getImmediateRenderer();

    const auto& vertices = disc.getVertices();
    const auto& indices = disc.getIndices();

    renderer->pushMatrix();
    renderer->translate(m_center);
    renderer->scale((float)m_radius);

    renderer->begin(PrimitiveType_Line);

    for (int i = 0; i < indices.size(); ++i) {
        const Vertex& v0 = vertices[indices[i]];
        renderer->vertex(v0.position);
    }

    renderer->end();
    renderer->popMatrix();
}

void BoundingSphere::drawFill() const {
    static MeshData<Vertex> disc(PrimitiveType_Triangle);
    if (disc.getVertexCount() == 0) {
        disc.createUVSphere(glm::vec3(0.0F), 1.0F, 18, 18);
    }

    ImmediateRenderer* renderer = Engine::instance()->getImmediateRenderer();

    const auto& vertices = disc.getVertices();
    const auto& indices = disc.getIndices();

    renderer->pushMatrix();
    renderer->translate(m_center);
    renderer->scale((float)m_radius);

    renderer->begin(PrimitiveType_Triangle);

    for (int i = 0; i < indices.size(); ++i) {
        const Vertex& v0 = vertices[indices[i]];
        renderer->normal(v0.normal);
        renderer->vertex(v0.position);
    }

    renderer->end();
    renderer->popMatrix();
}






AxisAlignedBoundingBox::AxisAlignedBoundingBox():
    BoundingVolume(Type_AxisAlignedBoundingBox),
    m_center(0.0, 0.0, 0.0),
    m_halfExtents(0.0, 0.0, 0.0) {
}

AxisAlignedBoundingBox::AxisAlignedBoundingBox(const glm::dvec3& center, const glm::dvec3& halfExtents):
        BoundingVolume(Type_AxisAlignedBoundingBox),
        m_center(center),
        m_halfExtents(glm::abs(halfExtents)) {

}

AxisAlignedBoundingBox::~AxisAlignedBoundingBox() = default;

glm::dvec3 AxisAlignedBoundingBox::getCenter() const {
    return m_center;
}

void AxisAlignedBoundingBox::setCenter(const glm::dvec3& center) {
    m_center = center;
}

void AxisAlignedBoundingBox::setCenter(double centerX, double centerY, double centerZ) {
    m_center.x = centerX;
    m_center.y = centerY;
    m_center.z = centerZ;
}

const glm::dvec3& AxisAlignedBoundingBox::getHalfExtents() const {
    return m_halfExtents;
}

void AxisAlignedBoundingBox::setHalfExtents(const glm::dvec3& halfExtents) {
    m_halfExtents = glm::abs(halfExtents);
}

void AxisAlignedBoundingBox::setHalfExtents(double halfExtentX, double halfExtentY, double halfExtentZ) {
    m_halfExtents.x = glm::abs(halfExtentX);
    m_halfExtents.y = glm::abs(halfExtentY);
    m_halfExtents.z = glm::abs(halfExtentZ);
}

glm::dvec3 AxisAlignedBoundingBox::getBoundMin() const {
    return m_center - m_halfExtents;
}

glm::dvec3 AxisAlignedBoundingBox::getBoundMax() const {
    return m_center + m_halfExtents;
}

void AxisAlignedBoundingBox::setBoundMinMax(const glm::dvec3& boundMin, const glm::dvec3& boundMax) {
    m_center = (boundMin + boundMax) * 0.5;
    m_halfExtents = glm::abs(boundMax - boundMin) * 0.5;
}

bool AxisAlignedBoundingBox::intersects(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return intersects_Sphere_AABB(cast<const BoundingSphere&>(other), *this);

        case Type_AxisAlignedBoundingBox:
            return intersects_AABB_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return false;
    }
}

bool AxisAlignedBoundingBox::contains(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return contains_AABB_Sphere(*this, cast<const BoundingSphere&>(other));

        case Type_AxisAlignedBoundingBox:
            return contains_AABB_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return false;
    }
}

bool AxisAlignedBoundingBox::contains(const glm::dvec3& other) const {
    return contains_AABB_Point(*this, other);
}

double AxisAlignedBoundingBox::calculateMinDistance(const BoundingVolume& other) const {
    switch (other.getType()) {
        case Type_Sphere:
            return calculateMinDistance_Sphere_AABB(cast<const BoundingSphere&>(other), *this);

        case Type_AxisAlignedBoundingBox:
            return calculateMinDistance_AABB_AABB(*this, cast<const AxisAlignedBoundingBox&>(other));

        case Type_OrientedBoundingBox:

        case Type_Cylinder:

        case Type_Capsule:

        default:
            assert(false);
            return 0.0;
    }
}

double AxisAlignedBoundingBox::calculateMinDistance(const glm::dvec3& other) const {
    return calculateMinDistance_AABB_Point(*this, other);
}

glm::dvec3 AxisAlignedBoundingBox::calculateClosestPoint(const glm::dvec3& point) const {
    return calculateClosestPoint_AABB_Point(*this, point);
}