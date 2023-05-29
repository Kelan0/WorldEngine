
#include "Plane.h"

Plane::Plane():
    normal(0.0, 0.0, 0.0),
    offset(0.0) { // Initialize as degenerate plane (zero-length normal)
}

Plane::Plane(double a, double b, double c, double d):
    normal(a, b, c), offset(d) {
}

Plane::Plane(const glm::dvec3& point, const glm::dvec3& normal):
    normal(normal),
    offset(glm::dot(normal, point)) {
}

Plane::Plane(const Plane& copy) = default;

Plane::Plane(Plane&& move) noexcept:
    normal(std::exchange(move.normal, glm::dvec3(0.0))),
    offset(std::exchange(move.offset, 0.0)) {
}

Plane& Plane::operator=(const Plane& copy) = default;

Plane& Plane::operator=(Plane&& move)  noexcept {
    normal = std::exchange(move.normal, glm::dvec3(0.0));
    offset = std::exchange(move.offset, 0.0);
    return *this;
}

bool Plane::isDegenerate() const {
    return lengthSquared() < 1e-12; // Effectively zero-length normal
}

Plane Plane::normalize(const Plane& plane) {
    Plane copy(plane);
    copy.normalize();
    return copy;
}

Plane& Plane::normalize() {
    double d0 = lengthSquared();
    if (d0 < 1e-12)
        return *this;
    d0 = glm::inversesqrt(d0);
    normal *= d0;
    offset *= d0;
    return *this;
}

double Plane::lengthSquared(const Plane& plane) {
    return plane.lengthSquared();
}

double Plane::lengthSquared() const {
    return glm::dot(normal, normal);
}

double Plane::length(const Plane& plane) {
    return plane.length();
}

double Plane::length() const {
    return glm::sqrt(lengthSquared());
}

glm::dvec3 Plane::getOrigin() const {
    return normal * offset;
}

const glm::dvec3& Plane::getNormal() const {
    return normal;
}

double Plane::getOffset() const {
    return offset;
}

double& Plane::operator[](glm::length_t index) {
    assert(index >= 0 && index < 4);
    switch(index) {
        default:
        case 0: return normal.x;
        case 1: return normal.y;
        case 2: return normal.z;
        case 3: return offset;
    }
}

double const& Plane::operator[](glm::length_t index) const {
    assert(index >= 0 && index < 4);
    switch(index) {
        default:
        case 0: return normal.x;
        case 1: return normal.y;
        case 2: return normal.z;
        case 3: return offset;
    }
}

double Plane::calculateSignedDistance(const glm::dvec3& point) const {
    return glm::dot(normal, point) + offset;
}

double Plane::calculateMinSignedDistance(const BoundingVolume& boundingVolume) const {
    switch (boundingVolume.getType()) {
        case BoundingVolume::Type_Sphere: {
            const BoundingSphere& sphere = BoundingVolume::cast<const BoundingSphere&>(boundingVolume);
            return calculateSignedDistance(sphere.getCenter()) - sphere.getRadius();
        }
        case BoundingVolume::Type_AxisAlignedBoundingBox: {
            const AxisAlignedBoundingBox& aabb = BoundingVolume::cast<const AxisAlignedBoundingBox&>(boundingVolume);
            const glm::dvec3 halfExtents = aabb.getHalfExtents();
            double maxProjectedExtent = glm::abs(halfExtents.x * normal.x) + glm::abs(halfExtents.y * normal.y) + glm::abs(halfExtents.z * normal.z);
            return calculateSignedDistance(aabb.getCenter()) - maxProjectedExtent;
        }
        case BoundingVolume::Type_OrientedBoundingBox:
        case BoundingVolume::Type_Cylinder:
        case BoundingVolume::Type_Capsule:
        default:
            assert(false);
            return 0.0;
    }
}

double Plane::calculateMaxSignedDistance(const BoundingVolume& boundingVolume) const {
    switch (boundingVolume.getType()) {
        case BoundingVolume::Type_Sphere: {
            const BoundingSphere& sphere = BoundingVolume::cast<const BoundingSphere&>(boundingVolume);
            return calculateSignedDistance(sphere.getCenter()) + sphere.getRadius();
        }
        case BoundingVolume::Type_AxisAlignedBoundingBox: {
            const AxisAlignedBoundingBox& aabb = BoundingVolume::cast<const AxisAlignedBoundingBox&>(boundingVolume);
            const glm::dvec3 halfExtents = aabb.getHalfExtents();
            double maxProjectedExtent = glm::abs(halfExtents.x * normal.x) + glm::abs(halfExtents.y * normal.y) + glm::abs(halfExtents.z * normal.z);
            return calculateSignedDistance(aabb.getCenter()) + maxProjectedExtent;
        }
        case BoundingVolume::Type_OrientedBoundingBox:
        case BoundingVolume::Type_Cylinder:
        case BoundingVolume::Type_Capsule:
        default:
            assert(false);
            return 0.0;
    }
}

bool Plane::intersects(const BoundingVolume& boundingVolume) const {
    switch (boundingVolume.getType()) {
        case BoundingVolume::Type_Sphere: {
            const BoundingSphere& sphere = BoundingVolume::cast<const BoundingSphere&>(boundingVolume);
            return glm::abs(calculateSignedDistance(sphere.getCenter())) < sphere.getRadius();
        }
        case BoundingVolume::Type_AxisAlignedBoundingBox: {
            const AxisAlignedBoundingBox& aabb = BoundingVolume::cast<const AxisAlignedBoundingBox&>(boundingVolume);
            const glm::dvec3 halfExtents = aabb.getHalfExtents();
            double maxProjectedExtent = glm::abs(halfExtents.x * normal.x) + glm::abs(halfExtents.y * normal.y) + glm::abs(halfExtents.z * normal.z);
            return glm::abs(calculateSignedDistance(aabb.getCenter())) < maxProjectedExtent;
        }
        case BoundingVolume::Type_OrientedBoundingBox:
        case BoundingVolume::Type_Cylinder:
        case BoundingVolume::Type_Capsule:
        default:
            assert(false);
            return false;
    }
}

glm::dvec3 Plane::triplePlaneIntersection(const Plane& a, const Plane& b, const Plane& c) {
    glm::dvec3 axb = glm::cross(a.getNormal(), b.getNormal());
    glm::dvec3 bxc = glm::cross(b.getNormal(), c.getNormal());
    glm::dvec3 cxa = glm::cross(c.getNormal(), a.getNormal());
    glm::dvec3 r = -a.getOffset() * bxc - b.getOffset() * cxa - c.getOffset() * axb;
    return r * (1.0 / dot(a.getNormal(), bxc));
}

Plane Plane::transform(const Plane& plane, const glm::dmat4& matrix, bool skewMatrix) {
    glm::dvec4 origin = glm::dvec4(plane.getOrigin(), 1.0);
    glm::dvec4 normal = glm::dvec4(plane.getNormal(), 0.0);

    origin = matrix * origin;
    if (skewMatrix) {
        normal = glm::transpose(glm::inverse(matrix)) * normal;
    } else {
        normal = matrix * normal;
    }

    return Plane(origin, normal);
}
