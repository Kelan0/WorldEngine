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

    Plane(Plane&& move);

    bool isDegenerate() const;

    static Plane normalize(const Plane& plane);

    Plane& normalize();

    static double lengthSquared(const Plane& plane);

    double lengthSquared() const;

    static double length(const Plane& plane);

    double length() const;

    const glm::dvec3& getNormal() const;

    double getOffset() const;

    double calculateSignedDistance(const glm::dvec3& point) const;

    double calculateMinSignedDistance(const BoundingVolume& boundingVolume) const;

    double calculateMaxSignedDistance(const BoundingVolume& boundingVolume) const;

    bool intersects(const BoundingVolume& boundingVolume) const;

    // Fast triple-plane intersection point, assumes that the planes WILL intersect
    // at a single point, and none are parallel, or intersect along a line.
    static glm::dvec3 triplePlaneIntersection(const Plane& a, const Plane& b, const Plane& c);

public:
    union {
        struct {
            glm::dvec3 normal;
            double offset;
        };
        struct {
            double a, b, c, d;
        };
    };
};


#endif //WORLDENGINE_PLANE_H
