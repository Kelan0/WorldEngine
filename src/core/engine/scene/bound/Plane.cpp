
#include "Plane.h"

Plane::Plane():
    a(0.0), b(0.0), c(0.0), d(0.0) { // Initialize as degenerate plane (zero-length normal)
}

Plane::Plane(const double& a, const double& b, const double& c, const double& d):
    a(a), b(b), c(c), d(d) {
}

Plane::Plane(const glm::dvec3& point, const glm::dvec3& normal):
    a(normal.x),
    b(normal.y),
    c(normal.z),
    d(-1 * glm::dot(normal, point)) {
}

Plane::Plane(const Plane& copy):
    a(copy.a),
    b(copy.b),
    c(copy.c),
    d(copy.d) {
}

Plane::Plane(Plane&& move):
    a(std::exchange(move.a, 0.0)),
    b(std::exchange(move.b, 0.0)),
    c(std::exchange(move.c, 0.0)),
    d(std::exchange(move.d, 0.0)) {
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
    a *= d0;
    b *= d0;
    c *= d0;
    d *= d0;
    return *this;
}

double Plane::lengthSquared(const Plane& plane) {
    return plane.lengthSquared();
}

double Plane::lengthSquared() const {
    return a * a + b * b + c * c;
}

double Plane::length(const Plane& plane) {
    return plane.length();
}

double Plane::length() const {
    return glm::sqrt(lengthSquared());
}

glm::dvec3 Plane::getNormal() const {
    return glm::dvec3(a, b, c);
}

double Plane::getOffset() const {
    return d;
}

double Plane::signedDistance(const glm::dvec3& point) const {
    return signedDistance(point.x, point.y, point.z);
}

double Plane::signedDistance(const double& x, const double& y, const double& z) const {
    return a * x + b * y + c * z + d;
}

glm::dvec3 Plane::triplePlaneIntersection(const Plane& a, const Plane& b, const Plane& c) {

    glm::dvec3 axb = glm::cross(a.getNormal(), b.getNormal());
    glm::dvec3 bxc = glm::cross(b.getNormal(), c.getNormal());
    glm::dvec3 cxa = glm::cross(c.getNormal(), a.getNormal());
    glm::dvec3 r = -a.getOffset() * bxc - b.getOffset() * cxa - c.getOffset() * axb;
    return r * (1.0 / dot(a.getNormal(), bxc));
}
