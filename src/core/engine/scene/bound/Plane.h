#ifndef WORLDENGINE_PLANE_H
#define WORLDENGINE_PLANE_H

#include "core/core.h"
#include "BoundingVolume.h"

class Plane {
public:
//    enum Side {
//        Side_Positive = 1,
//        Side_Inside = 0,
//        Side_Negative = -1,
//    };
public:
    Plane();

    Plane(double a, double b, double c, double d);

    Plane(const glm::dvec3& point, const glm::dvec3& normal);

    Plane(const Plane& copy);

    Plane(Plane&& move) noexcept ;

    Plane& operator=(const Plane& copy);

    Plane& operator=(Plane&& move) noexcept;

    bool isDegenerate() const;

    static Plane normalize(const Plane& plane);

    Plane& normalize();

    static double lengthSquared(const Plane& plane);

    double lengthSquared() const;

    static double length(const Plane& plane);

    double length() const;

    glm::dvec3 getOrigin() const;

    const glm::dvec3& getNormal() const;

    double getOffset() const;

    double& operator[](glm::length_t index);

    double const& operator[](glm::length_t index) const;

    double calculateSignedDistance(const glm::dvec3& point) const;

    double calculateMinSignedDistance(const BoundingVolume& boundingVolume) const;

    double calculateMaxSignedDistance(const BoundingVolume& boundingVolume) const;

    bool intersects(const BoundingVolume& boundingVolume) const;

    // Fast triple-plane intersection point, assumes that the planes WILL intersect
    // at a single point, and none are parallel, or intersect along a line.
    static glm::dvec3 triplePlaneIntersection(const Plane& a, const Plane& b, const Plane& c);

    static Plane transform(const Plane& plane, const glm::dmat4& matrix, bool skewMatrix = false);

public:
    glm::dvec3 normal;
    double offset;
};


#endif //WORLDENGINE_PLANE_H
