#include "Camera.h"

Camera::Camera():
        Camera(glm::half_pi<double>(), 1.0, 0.1, 100.0) {
}

Camera::Camera(double fov, double aspect, double near, double far) {
    setPerspective(fov, aspect, near, far);
}

Camera::Camera(double left, double right, double bottom, double top, double near, double far, bool isOrtho) {
    if (isOrtho)
        setOrtho(left, right, bottom, top, near, far);
    else
        setPerspective(left, right, bottom, top, near, far);
}

Camera& Camera::setPerspective(double fov, double aspect, double near, double far) {
    double halfHeight = glm::tan(fov * 0.5) * near * 0.5;
    double halfWidth = halfHeight * aspect;
    setPerspective(-halfWidth, halfWidth, -halfHeight, halfHeight, near, far);
    return *this;
}

Camera& Camera::setPerspective(double left, double right, double bottom, double top, double near, double far) {
    set(left, right, bottom, top, near, far, false);
    return *this;
}

Camera& Camera::setOrtho(double left, double right, double bottom, double top, double near, double far) {
    set(left, right, bottom, top, near, far, true);
    return *this;
}

Camera& Camera::setFov(double fov) {
    if (!m_isOrtho)
        setPerspective(fov, getAspect(), getNear(), getFar());
    return *this;
}

Camera& Camera::setFovDegrees(double fov) {
    return setFov(glm::radians(fov));
}

Camera& Camera::setAspect(double aspect) {
    if (m_isOrtho) {
        // Set orthographic aspect ratio. Maintain height, center and orientation
        double newWidth = glm::abs(m_top - m_bottom) * aspect;
        double center = (m_left + m_right) * 0.5;
        double left = center - newWidth * 0.5;
        double right = center + newWidth * 0.5;
        if (m_left > m_right) std::swap(left, right);
        setOrtho(left, right, m_bottom, m_top, m_near, m_far);
    } else {
        setPerspective(getFov(), aspect, getNear(), getFar());
    }
    return *this;
}

Camera& Camera::setClippingPlanes(double near, double far) {
    setPerspective(getFov(), getAspect(), near, far);
    return *this;
}

double Camera::getFov() const {
    return glm::atan(glm::abs(m_top - m_bottom) / m_near) * 2.0;
}

double Camera::getFovDegrees() const {
    return glm::degrees(getFov());
}

double Camera::getAspect() const {
    return glm::abs(m_right - m_left) / glm::abs(m_top - m_bottom);
}

double Camera::getLeft() const {
    return m_left;
}

double Camera::getRight() const {
    return m_right;
}

double Camera::getBottom() const {
    return m_bottom;
}

double Camera::getTop() const {
    return m_top;
}

double Camera::getNear() const {
    return m_near;
}

double Camera::getFar() const {
    return m_far;
}

glm::mat4 Camera::getProjectionMatrix() const {
    if (m_isOrtho) {
        return glm::ortho(m_left, m_right, m_bottom, m_top, m_near, m_far);
    } else {
        return glm::frustum(m_left, m_right, m_bottom, m_top, m_near, m_far);
    }
}

bool Camera::isOrtho() const {
    return m_isOrtho;
}

Camera& Camera::operator=(const Camera& other) {
    set(other.m_left, other.m_right, other.m_bottom, other.m_top, other.m_near, other.m_far, other.m_isOrtho);
    return *this;
}

bool Camera::operator==(const Camera& other) const {
    if (glm::epsilonNotEqual(m_left, other.m_left, 1e-6)) return false;
    if (glm::epsilonNotEqual(m_right, other.m_right, 1e-6)) return false;
    if (glm::epsilonNotEqual(m_bottom, other.m_bottom, 1e-6)) return false;
    if (glm::epsilonNotEqual(m_top, other.m_top, 1e-6)) return false;
    if (glm::epsilonNotEqual(m_near, other.m_near, 1e-6)) return false;
    if (glm::epsilonNotEqual(m_far, other.m_far, 1e-6)) return false;
    if (m_isOrtho != other.m_isOrtho) return false;
    return true;
}

bool Camera::operator!=(const Camera& other) const {
    return !(*this == other);
}

Camera::operator glm::mat4() const {
    return getProjectionMatrix();
}

void Camera::set(double left, double right, double bottom, double top, double near, double far, bool ortho) {
    m_left = left;
    m_right = right;
    m_bottom = bottom;
    m_top = top;
    m_near = near;
    m_far = far;
    m_isOrtho = ortho;
}
