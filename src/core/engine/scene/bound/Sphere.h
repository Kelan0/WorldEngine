#ifndef WORLDENGINE_SPHERE_H
#define WORLDENGINE_SPHERE_H

#include "core/core.h"

class Sphere {
public:
    Sphere();

    Sphere(const glm::dvec3& centre, const double& radius);

    Sphere(const double& x, const double& y, const double& z, const double& radius);

    Sphere(const Sphere& copy);

    Sphere(Sphere&& move);

    glm::dvec3 getCentre() const;

    Sphere& setCentre(const glm::dvec3& centre);

    Sphere& setCentre(const double& x, const double& y, const double& z);

    double getRadius() const;

    Sphere& setRadius(const double& radius);

    bool containsPoint(const glm::dvec3& point) const;

    bool containsPoint(const double& x, const double& y, const double& z) const;

    double signedDistance(const glm::dvec3& point) const;

    double signedDistance(const double& x, const double& y, const double& z) const;

    double centreDistanceSquared(const glm::dvec3& point) const;

    double centreDistanceSquared(const double& x, const double& y, const double& z) const;

    double centreDistance(const glm::dvec3& point) const;

    double centreDistance(const double& x, const double& y, const double& z) const;
public:
    double x;
    double y;
    double z;
    double radius;
};


#endif //WORLDENGINE_SPHERE_H
