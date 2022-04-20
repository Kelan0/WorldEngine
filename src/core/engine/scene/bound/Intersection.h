#ifndef WORLDENGINE_INTERSECTION_H
#define WORLDENGINE_INTERSECTION_H

#include "core/core.h"
#include "Plane.h"
#include "Sphere.h"
#include "Frustum.h"

typedef glm::dvec3 Point;

namespace Intersection {
    // Check if A and B are intersecting
    template<typename A, typename B>
    static bool intersects(const A& a, const B& b);

    // Check if A fully contains B
    template<typename A, typename B>
    static bool contains(const A& a, const B& b);

    //template<typename A, typename B>
    //static double minDistance(const A& a, const B& b);
};


template<>
bool Intersection::intersects<Sphere, Sphere>(const Sphere& a, const Sphere& b) {
    return a.centreDistance(b.getCentre()) < a.getRadius() + b.getRadius();
}

template<>
bool Intersection::contains<Sphere, Sphere>(const Sphere& a, const Sphere& b) {
    return a.centreDistance(b.getCentre()) + b.getRadius() <= a.getRadius();
}



template<>
bool Intersection::intersects<Sphere, Point>(const Sphere& a, const Point& b) {
    return a.centreDistanceSquared(b) <= a.getRadius() * a.getRadius();
}

template<>
bool Intersection::contains<Sphere, Point>(const Sphere& a, const Point& b) {
    return a.centreDistanceSquared(b) < a.getRadius() * a.getRadius();
}

template<>
bool Intersection::intersects<Point, Sphere>(const Point& a, const Sphere& b) {
    return Intersection::intersects(b, a);
}

template<>
bool Intersection::contains<Point, Sphere>(const Point& a, const Sphere& b) {
    return false; // Point is zero-dimensional. Can't contain sphere
}



template<>
bool Intersection::intersects<Sphere, Plane>(const Sphere& a, const Plane& b) {
    return glm::abs(b.signedDistance(a.getCentre())) < a.getRadius();
}

template<>
bool Intersection::contains<Sphere, Plane>(const Sphere& a, const Plane& b) {
    return intersects(a, b);
}

template<>
bool Intersection::intersects<Plane, Sphere>(const Plane& a, const Sphere& b) {
    return Intersection::intersects(b, a);
}

template<>
bool Intersection::contains<Plane, Sphere>(const Plane& a, const Sphere& b) {
    return Intersection::contains(b, a);
}



template<>
bool Intersection::intersects<Frustum, Sphere>(const Frustum& a, const Sphere& b) {
    for (size_t i = 0; i < 6; ++i)
        if (a.getPlane(i).signedDistance(b.getCentre()) < b.getRadius() * -1)
            return false; // The sphere is entirely on the negative side of this plane, therefor outside the frustum
    return true;
}

template<>
bool Intersection::contains<Frustum, Sphere>(const Frustum& a, const Sphere& b) {
    for (size_t i = 0; i < Frustum::NumPlanes; ++i)
        if (a.getPlane(i).signedDistance(b.getCentre()) < b.getRadius())
            return false; // The sphere is not fully on the positive side of this plane, therefor the sphere is not fully inside the frustum.
    return true;
}

template<>
bool Intersection::intersects<Sphere, Frustum>(const Sphere& a, const Frustum& b) {
    return Intersection::intersects(b, a);
}

template<>
bool Intersection::contains<Sphere, Frustum>(const Sphere& a, const Frustum& b) {
    for (size_t i = 0; i < Frustum::NumCorners; ++i)
        if (!a.containsPoint(b.getCorner(i)))
            return false; // One or more of the frustum corners are outside the sphere, therefor sphere does not fully contain the frustum.
    return true;
}


#endif //WORLDENGINE_INTERSECTION_H
