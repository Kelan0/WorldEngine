#ifndef WORLDENGINE_PLANE_H
#define WORLDENGINE_PLANE_H

#include "core/core.h"

class Plane {
public:
//    enum Side {
//        Side_Positive = 1,
//        Side_Inside = 0,
//        Side_Negative = -1,
//    };
public:
    Plane();

    Plane(const double& a, const double& b, const double& c, const double& d);

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

    glm::dvec3 getNormal() const;

    double getOffset() const;

    double signedDistance(const glm::dvec3& point) const;

    double signedDistance(const double& x, const double& y, const double& z) const;

    // Fast triple-plane intersection point, assumes that the planes WILL intersect
    // at a single point, and none are parallel, or intersect along a line.
    static glm::dvec3 triplePlaneIntersection(const Plane& a, const Plane& b, const Plane& c);

public:
    double a;
    double b;
    double c;
    double d;
};


#endif //WORLDENGINE_PLANE_H
