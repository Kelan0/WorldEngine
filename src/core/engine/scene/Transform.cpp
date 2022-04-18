#include "core/engine/scene/Transform.h"
#include "core/application/Application.h"
#include "core/engine/renderer/SceneRenderer.h"


Transform::Transform():
        m_translation(0.0),
        m_rotation(1.0),
        m_scale(1.0) {
}

Transform::Transform(const glm::mat4& transformationMatrix) {
    setMatrix(transformationMatrix);
}

Transform::Transform(const Transform& copy):
        m_translation(copy.m_translation),
        m_rotation(copy.m_rotation),
        m_scale(copy.m_scale) {
    change();
}

Transform::Transform(Transform&& move) noexcept:
        m_translation(std::exchange(move.m_translation, glm::dvec3(0.0))),
        m_rotation(std::exchange(move.m_rotation, glm::dmat3(1.0))),
        m_scale(std::exchange(move.m_scale, glm::dvec3(1.0))) {
    change();
}

Transform::~Transform() {
}

const glm::dvec3& Transform::getTranslation() const {
    return m_translation;
}

const double& Transform::getX() const {
    return m_translation.x;
}

const double& Transform::getY() const {
    return m_translation.y;
}

const double& Transform::getZ() const {
    return m_translation.z;
}

glm::quat Transform::getRotation() const {
    return glm::quat_cast(m_rotation);
}

const glm::mat3& Transform::getRotationMatrix() const {
    return m_rotation;
}

const glm::vec3& Transform::getXAxis() const {
    return m_rotation[0];
}

glm::vec3 Transform::getLeftAxis() const {
    return -1.0F * getXAxis();
}

glm::vec3 Transform::getRightAxis() const {
    return getXAxis();
}

const glm::vec3& Transform::getYAxis() const {
    return m_rotation[1];
}

glm::vec3 Transform::getDownAxis() const {
    return -1.0F * getYAxis();
}

glm::vec3 Transform::getUpAxis() const {
    return getYAxis();
}

const glm::vec3& Transform::getZAxis() const {
    return m_rotation[2];
}

glm::vec3 Transform::getForwardAxis() const {
    return -1.0F * getZAxis();
}

glm::vec3 Transform::getBackwardAxis() const {
    return getZAxis();
}

glm::vec3 Transform::getEulerAngles() const {
    glm::vec3 angles;
    glm::extractEulerAngleYXZ(glm::mat4(m_rotation), angles.x, angles.y, angles.z);
    return angles;
}

float Transform::getPitch() const {
    return getEulerAngles().x;
}

float Transform::getYaw() const {
    return getEulerAngles().y;
}

float Transform::getRoll() const {
    return getEulerAngles().z;
}

const glm::dvec3& Transform::getScale() const {
    return m_scale;
}

const double& Transform::getScaleX() const {
    return m_scale.x;
}

const double& Transform::getScaleY() const {
    return m_scale.y;
}

const double& Transform::getScaleZ() const {
    return m_scale.z;
}

Transform& Transform::setTranslation(const glm::dvec4& translation) {
    m_translation = glm::dvec3(translation);
    change();
    return *this;
}

Transform& Transform::setTranslation(const glm::dvec3& translation) {
    m_translation = translation;
    change();
    return *this;
}

Transform& Transform::setTranslation(const glm::dvec2& translation) {
    m_translation = glm::dvec3(translation, 0.0);
    change();
    return *this;
}

Transform& Transform::setTranslation(double x, double y, double z) {
    m_translation = glm::dvec3(x, y, z);
    change();
    return *this;
}

Transform& Transform::setTranslation(double x, double y) {
    m_translation = glm::dvec3(x, y, 0.0);
    change();
    return *this;
}

Transform& Transform::setRotation(const glm::mat3& rotation) {
    m_rotation = rotation;
    change();
    return *this;
}

Transform& Transform::setRotation(const glm::mat4& rotation) {
    m_rotation = glm::mat3(rotation);
    change();
    return *this;
}

Transform& Transform::setRotation(const glm::vec3& forward, const glm::vec3& up, bool normalized) {
    constexpr float eps = 1e-4;
    constexpr float epsSq = eps * eps;
    if (glm::dot(forward, forward) < epsSq) {
        m_rotation = glm::mat3(1.0); // zero-length forward vector, set rotation to identity
        return *this;
    }
    if (glm::dot(up, up) < epsSq) {
        m_rotation = glm::mat3(1.0); // zero-length up vector, set rotation to identity
        return *this;
    }

    m_rotation[2] = -1.0F * (normalized ? forward : glm::normalize(forward));
    glm::vec3 const& right = glm::cross(up, m_rotation[2]); // cross(Y, Z)
    m_rotation[0] = right * glm::inversesqrt(glm::max(eps, glm::dot(right, right)));
    m_rotation[1] = glm::cross(m_rotation[2], m_rotation[0]); // cross(Z, X)
    change();
    return *this;
}

Transform& Transform::setRotation(const glm::quat& rotation, bool normalized) {
    m_rotation = (normalized) ? (glm::mat3_cast(rotation)) : (glm::mat3_cast(glm::normalize(rotation)));
    change();
    return *this;
}

Transform& Transform::setRotation(const glm::vec3& eulerAngles) {
    setRotation(eulerAngles.x, eulerAngles.y, eulerAngles.z);
    return *this;
}

Transform& Transform::setRotation(float pitch, float yaw, float roll) {
    //setRotation(glm::yawPitchRoll(yaw, pitch, roll));
    setRotation(glm::eulerAngleYXZ(yaw, pitch, roll));
    return *this;
}

Transform& Transform::setRotation(float pitch, float yaw) {
    setRotation(pitch, yaw, 0.0);
    return *this;
}

Transform& Transform::setScale(const glm::dvec3& scale) {
    m_scale = scale;
    change();
    return *this;
}

Transform& Transform::setScale(double x, double y, double z) {
    setScale(glm::dvec3(x, y, z));
    return *this;
}

Transform& Transform::setScale(double scale) {
    setScale(glm::dvec3(scale));
    return *this;
}

Transform& Transform::translate(const glm::dvec4& translation) {
    translate(translation.x, translation.y, translation.z);
    return *this;
}

Transform& Transform::translate(const glm::dvec3& translation) {
    translate(translation.x, translation.y, translation.z);
    return *this;
}

Transform& Transform::translate(const glm::dvec2& translation) {
    translate(translation.x, translation.y);
    return *this;
}

Transform& Transform::translate(double x, double y, double z) {
    m_translation.x += x;
    m_translation.y += y;
    m_translation.z += z;
    change();
    return *this;
}

Transform& Transform::translate(double x, double y) {
    m_translation.x += x;
    m_translation.y += y;
    change();
    return *this;
}

Transform& Transform::rotate(const glm::quat& rotation, bool normalized) {
    normalized
    ? setRotation(rotation * getRotation())
    : setRotation(glm::normalize(rotation) * getRotation());
    return *this;
}

Transform& Transform::rotate(const glm::mat3& rotation, bool normalized) {
    if (normalized) {
        m_rotation = rotation * m_rotation;
    } else {
        m_rotation = glm::mat3(
                glm::normalize(m_rotation[0]),
                glm::normalize(m_rotation[1]),
                glm::normalize(m_rotation[2])
        ) * m_rotation;
    }
    change();
    return *this;
}

Transform& Transform::rotate(const glm::mat4& rotation, bool normalized) {
    rotate(glm::dmat3(rotation), normalized);
    return *this;
}

Transform& Transform::rotate(const glm::vec3& axis, float angle) {
    m_rotation = glm::mat3(glm::rotate(glm::mat4(m_rotation), angle, axis));
    change();
    return *this;
}

Transform& Transform::rotate(const glm::vec4& axisAngle) {
    rotate(glm::dvec3(axisAngle.x, axisAngle.y, axisAngle.z), axisAngle.w);
    return *this;
}

Transform& Transform::rotate(float x, float y, float z, float angle) {
    rotate(glm::dvec3(x, y, z), angle);
    return *this;
}

Transform& Transform::rotate(const glm::vec3& eulerAngles) {
    setRotation(getEulerAngles() + eulerAngles);
    return *this;
}

Transform& Transform::rotate(float pitch, float yaw, float roll) {
    setRotation(getEulerAngles() + glm::vec3(pitch, yaw, roll));
    return *this;
}

Transform& Transform::scale(const glm::dvec3& scale) {
    m_scale *= scale;
    change();
    return *this;
}

Transform& Transform::scale(double x, double y, double z) {
    m_scale.x *= x;
    m_scale.y *= y;
    m_scale.z *= z;
    change();
    return *this;
}

Transform& Transform::scale(double scale) {
    m_scale.x *= scale;
    m_scale.y *= scale;
    m_scale.z *= scale;
    change();
    return *this;
}

Transform Transform::operator*(const glm::dmat4& other) const {
    return Transform(getMatrix() * other);
}

Transform Transform::operator*(const Transform& other) const {
    return Transform(getMatrix() * other.getMatrix());
}

Transform& Transform::operator=(glm::dmat4 other) {
    setMatrix(other);
    return *this;
}

Transform& Transform::operator=(const Transform& other) {
    m_translation = other.m_translation;
    m_rotation = other.m_rotation;
    m_scale = other.m_scale;
    change();
    return *this;
}

bool Transform::equalsTranslation(const Transform& other, double epsilon) const {
    if (!glm::epsilonEqual(m_translation.x, other.m_translation.x, epsilon)) return false;
    if (!glm::epsilonEqual(m_translation.y, other.m_translation.y, epsilon)) return false;
    if (!glm::epsilonEqual(m_translation.z, other.m_translation.z, epsilon)) return false;
    return true;
}

bool Transform::equalsRotation(const Transform& other, double epsilon) const {
    float feps = epsilon;
    if (!glm::epsilonEqual(m_rotation[0].x, other.m_rotation[0].x, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[1].x, other.m_rotation[1].x, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[2].x, other.m_rotation[2].x, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[0].y, other.m_rotation[0].y, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[1].y, other.m_rotation[1].y, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[2].y, other.m_rotation[2].y, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[0].z, other.m_rotation[0].z, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[1].z, other.m_rotation[1].z, feps)) return false;
    if (!glm::epsilonEqual(m_rotation[2].z, other.m_rotation[2].z, feps)) return false;
    return true;
}

bool Transform::equalsScale(const Transform& other, double epsilon) const {
    if (!glm::epsilonEqual(m_scale.x, other.m_scale.x, epsilon)) return false;
    if (!glm::epsilonEqual(m_scale.y, other.m_scale.y, epsilon)) return false;
    if (!glm::epsilonEqual(m_scale.z, other.m_scale.z, epsilon)) return false;
    return true;
}

bool Transform::equalsTranslation(const Transform& other) const {
    return m_translation == other.m_translation;
}

bool Transform::equalsRotation(const Transform& other) const {
    if (m_rotation[0].x != other.m_rotation[0].x) return false;
    if (m_rotation[1].x != other.m_rotation[1].x) return false;
    if (m_rotation[2].x != other.m_rotation[2].x) return false;
    if (m_rotation[0].y != other.m_rotation[0].y) return false;
    if (m_rotation[1].y != other.m_rotation[1].y) return false;
    if (m_rotation[2].y != other.m_rotation[2].y) return false;
    if (m_rotation[0].z != other.m_rotation[0].z) return false;
    if (m_rotation[1].z != other.m_rotation[1].z) return false;
    if (m_rotation[2].z != other.m_rotation[2].z) return false;
    return true;
}

bool Transform::equalsScale(const Transform& other) const {
    return m_scale == other.m_scale;
}

bool Transform::operator==(const Transform& other) const {
    if (!equalsTranslation(other)) return false;
    if (!equalsRotation(other)) return false;
    if (!equalsScale(other)) return false;
    return true;
}

bool Transform::operator==(const glm::dmat4& other) const {
    return (*this) == Transform(other);
}

bool Transform::operator==(const glm::mat4& other) const {
    return (*this) == Transform(other);
}

bool Transform::operator!=(const Transform& other) const {
    return !(*this == other);
}

bool Transform::operator!=(const glm::dmat4& other) const {
    return !(*this == other);
}

bool Transform::operator!=(const glm::mat4& other) const {
    return !(*this == other);
}

glm::dmat4 Transform::getMatrix() const {
    glm::dmat4 mat;
    return fillMatrix(mat);
}

glm::dmat4& Transform::fillMatrix(glm::dmat4& matrix) const {
    glm::vec3 scale = m_scale;
    matrix[0].x = m_rotation[0].x * scale.x;
    matrix[0].y = m_rotation[0].y * scale.x;
    matrix[0].z = m_rotation[0].z * scale.x;
    matrix[0].w = 0.0;
    matrix[1].x = m_rotation[1].x * scale.y;
    matrix[1].y = m_rotation[1].y * scale.y;
    matrix[1].z = m_rotation[1].z * scale.y;
    matrix[1].w = 0.0;
    matrix[2].x = m_rotation[2].x * scale.z;
    matrix[2].y = m_rotation[2].y * scale.z;
    matrix[2].z = m_rotation[2].z * scale.z;
    matrix[2].w = 0.0;
    matrix[3].x = m_translation.x;
    matrix[3].y = m_translation.y;
    matrix[3].z = m_translation.z;
    matrix[3].w = 1.0;
    return matrix;
}

glm::mat4& Transform::fillMatrix(glm::mat4& matrix) const {
    glm::vec3 scale = m_scale;
    matrix[0].x = m_rotation[0].x * scale.x;
    matrix[0].y = m_rotation[0].y * scale.x;
    matrix[0].z = m_rotation[0].z * scale.x;
    matrix[0].w = 0.0F;
    matrix[1].x = m_rotation[1].x * scale.y;
    matrix[1].y = m_rotation[1].y * scale.y;
    matrix[1].z = m_rotation[1].z * scale.y;
    matrix[1].w = 0.0F;
    matrix[2].x = m_rotation[2].x * scale.z;
    matrix[2].y = m_rotation[2].y * scale.z;
    matrix[2].z = m_rotation[2].z * scale.z;
    matrix[2].w = 0.0F;
    matrix[3].x = m_translation.x;
    matrix[3].y = m_translation.y;
    matrix[3].z = m_translation.z;
    matrix[3].w = 1.0F;
    return matrix;
}

glm::mat4x3& Transform::fillMatrix(glm::mat4x3& matrix) const {
    glm::vec3 scale = m_scale;
    matrix[0].x = m_rotation[0].x * scale.x;
    matrix[0].y = m_rotation[0].y * scale.x;
    matrix[0].z = m_rotation[0].z * scale.x;
    matrix[1].x = m_rotation[1].x * scale.y;
    matrix[1].y = m_rotation[1].y * scale.y;
    matrix[1].z = m_rotation[1].z * scale.y;
    matrix[2].x = m_rotation[2].x * scale.z;
    matrix[2].y = m_rotation[2].y * scale.z;
    matrix[2].z = m_rotation[2].z * scale.z;
    matrix[3].x = m_translation.x;
    matrix[3].y = m_translation.y;
    matrix[3].z = m_translation.z;
    return matrix;
}

Transform& Transform::setMatrix(const glm::dmat4& matrix) {
    m_translation = glm::dvec3(matrix[3]);
    m_rotation = glm::dmat3(matrix);
    m_scale.x = glm::length(m_rotation[0]);
    m_scale.y = glm::length(m_rotation[1]);
    m_scale.z = glm::length(m_rotation[2]);
    m_rotation[0] /= m_scale.x;
    m_rotation[1] /= m_scale.y;
    m_rotation[2] /= m_scale.z;
    change();
    return *this;
}

Transform::operator glm::dmat4() const {
    return getMatrix();
}

void Transform::change() {
    Application::instance()->renderer()->notifyTransformChanged(m_entityIndex);
}

void Transform::reindex(Transform& transform, const EntityChangeTracker::entity_index& newEntityIndex) {
    if (newEntityIndex == transform.m_entityIndex)
        return;
    transform.m_entityIndex = newEntityIndex;
}
