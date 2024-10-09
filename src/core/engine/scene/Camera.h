
#ifndef WORLDENGINE_CAMERA_H
#define WORLDENGINE_CAMERA_H

#include "core/core.h"

class Camera {
    friend class Frustum;
public:
    Camera();

    Camera(double fov, double aspect, double near, double far);

    Camera(double left, double right, double bottom, double top, double near, double far, bool isOrtho = false);

    Camera& setPerspective(double fov, double aspect, double near, double far);

    Camera& setPerspective(double left, double right, double bottom, double top, double near, double far);

    Camera& setOrtho(double left, double right, double bottom, double top, double near, double far);

    Camera& setFov(double fov);

    Camera& setFovDegrees(double fov);

    Camera& setAspect(double aspect);

    Camera& setClippingPlanes(double near, double far);

    double getFov() const;

    double getFovDegrees() const;

    double getAspect() const;

    double getLeft() const;

    double getRight() const;

    double getBottom() const;

    double getTop() const;

    double getNear() const;

    double getFar() const;

    glm::mat4 getProjectionMatrix() const;

    bool isOrtho() const;

    double calculateProjectedSize(double radius, double distance) const;

    static double calculateProjectedSize(double radius, double distance, bool isOrtho, double fov, double top, double bottom);

    static double calculateProjectedPerspectiveSize(double radius, double distance, double fov, double top, double bottom);

    static double calculateProjectedOrthographicSize(double radius, double top, double bottom);

    Camera& operator=(const Camera& other);

    bool operator==(const Camera& other) const;

    bool operator!=(const Camera& other) const;

    explicit operator glm::mat4() const;
private:
    void set(double left, double right, double bottom, double top, double near, double far, bool ortho);

private:
    double m_left;
    double m_right;
    double m_bottom;
    double m_top;
    double m_near;
    double m_far;
    bool m_isOrtho;
};

namespace std {
    template<> struct hash<Camera> {
        size_t operator()(const Camera& camera) const {
            size_t seed = 0;
            std::hash_combine(seed, camera.getLeft());
            std::hash_combine(seed, camera.getRight());
            std::hash_combine(seed, camera.getBottom());
            std::hash_combine(seed, camera.getTop());
            std::hash_combine(seed, camera.getNear());
            std::hash_combine(seed, camera.getFar());
            std::hash_combine(seed, camera.isOrtho());
            return seed;
        }
    };
}


#endif //WORLDENGINE_CAMERA_H
