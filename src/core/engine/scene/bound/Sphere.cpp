
#include "Sphere.h"


Sphere::Sphere():
    x(0.0), y(0.0), z(0.0), radius(0.0) {
}

Sphere::Sphere(const glm::dvec3& centre, const double& radius):
    x(centre.x), y(centre.y), z(centre.z), radius(radius) {
}

Sphere::Sphere(const double& x, const double& y, const double& z, const double& radius):
    x(x), y(y), z(z), radius(radius) {
}

Sphere::Sphere(const Sphere& copy):
    x(copy.x), y(copy.y), z(copy.z), radius(copy.radius) {
}

Sphere::Sphere(Sphere&& move):
    x(std::exchange(move.x, 0.0)),
    y(std::exchange(move.y, 0.0)),
    z(std::exchange(move.z, 0.0)),
    radius(std::exchange(move.radius, 0.0)) {
}

glm::dvec3 Sphere::getCentre() const {
    return glm::dvec3(x, y, z);
}

Sphere& Sphere::setCentre(const glm::dvec3& centre) {
    setCentre(centre.x, centre.y, centre.z);
    return *this;
}

Sphere& Sphere::setCentre(const double& x, const double& y, const double& z) {
    this->x = x;
    this->y = y;
    this->z = z;
    return *this;
}

double Sphere::getRadius() const {
    return radius;
}

Sphere& Sphere::setRadius(const double& radius) {
    this->radius = radius;
    return *this;
}

bool Sphere::containsPoint(const glm::dvec3& point) const {
    return containsPoint(point.x, point.y, point.z);
}

bool Sphere::containsPoint(const double& x, const double& y, const double& z) const {
    return centreDistanceSquared(x, y, z) < radius * radius;
}

double Sphere::signedDistance(const glm::dvec3& point) const {
    return signedDistance(point.x, point.y, point.z);
}

double Sphere::signedDistance(const double& x, const double& y, const double& z) const {
    return centreDistance(x, y, z) - radius;
}

double Sphere::centreDistanceSquared(const glm::dvec3& point) const {
    return centreDistanceSquared(point.x, point.y, point.z);
}

double Sphere::centreDistanceSquared(const double& x, const double& y, const double& z) const {
    double dx = x - this->x;
    double dy = y - this->y;
    double dz = z - this->z;
    return dx * dx + dy * dy + dz * dz;
}

double Sphere::centreDistance(const glm::dvec3& point) const {
    return centreDistance(point.x, point.y, point.z);
}

double Sphere::centreDistance(const double& x, const double& y, const double& z) const {
    return glm::sqrt(centreDistanceSquared(x, y, z));
}
