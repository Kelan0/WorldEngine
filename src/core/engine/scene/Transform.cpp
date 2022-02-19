#include "Transform.h"

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
}

Transform::Transform(Transform&& move) noexcept:
	m_translation(std::exchange(move.m_translation, glm::dvec3(0.0))),
	m_rotation(std::exchange(move.m_rotation, glm::dmat3(1.0))),
	m_scale(std::exchange(move.m_scale, glm::dvec3(1.0))) {
}

Transform::~Transform() {
}

glm::dvec3 Transform::getTranslation() const {
	return m_translation;
}

double Transform::getX() const {
	return m_translation.x;
}

double Transform::getY() const {
	return m_translation.y;
}

double Transform::getZ() const {
	return m_translation.z;
}

glm::dquat Transform::getRotation() const {
	return glm::quat_cast(m_rotation);
}

glm::dmat3 Transform::getRotationMatrix() const {
	return m_rotation;
}

glm::dvec3 Transform::getXAxis() const {
	return glm::dvec3(m_rotation[0]);
}

glm::dvec3 Transform::getLeftAxis() const {
	return -1.0 * getXAxis();
}

glm::dvec3 Transform::getRightAxis() const {
	return getXAxis();
}

glm::dvec3 Transform::getYAxis() const {
	return glm::dvec3(m_rotation[1]);
}

glm::dvec3 Transform::getDownAxis() const {
	return -1.0 * getYAxis();
}

glm::dvec3 Transform::getUpAxis() const {
	return getYAxis();
}

glm::dvec3 Transform::getZAxis() const {
	return glm::dvec3(m_rotation[2]);
}

glm::dvec3 Transform::getForwardAxis() const {
	return -1.0 * getZAxis();
}

glm::dvec3 Transform::getBackwardAxis() const {
	return getZAxis();
}

glm::dvec3 Transform::getEulerAngles() const {
	glm::dvec3 angles;
	glm::extractEulerAngleYXZ(glm::dmat4(m_rotation), angles.x, angles.y, angles.z);
	return angles;
}

double Transform::getPitch() const {
	return getEulerAngles().x;
}

double Transform::getYaw() const {
	return getEulerAngles().y;
}

double Transform::getRoll() const {
	return getEulerAngles().z;
}

glm::dvec3 Transform::getScale() const {
	return m_scale;
}

double Transform::getScaleX() const {
	return m_scale.x;
}

double Transform::getScaleY() const {
	return m_scale.y;
}

double Transform::getScaleZ() const {
	return m_scale.z;
}

Transform& Transform::setTranslation(const glm::dvec4& translation) {
	m_translation = glm::dvec3(translation);
	return *this;
}

Transform& Transform::setTranslation(const glm::dvec3& translation) {
	m_translation = translation;
	return *this;
}

Transform& Transform::setTranslation(const glm::dvec2& translation) {
	m_translation = glm::dvec3(translation, 0.0);
	return *this;
}

Transform& Transform::setTranslation(double x, double y, double z) {
	m_translation = glm::dvec3(x, y, z);
	return *this;
}

Transform& Transform::setTranslation(double x, double y) {
	m_translation = glm::dvec3(x, y, 0.0);
	return *this;
}

Transform& Transform::setRotation(const glm::dmat3& rotation) {
	m_rotation = rotation;
	return *this;
}

Transform& Transform::setRotation(const glm::dmat4& rotation) {
	m_rotation = glm::dmat3(rotation);
	return *this;
}

Transform& Transform::setRotation(const glm::dvec3& forward, const glm::dvec3& up, bool normalized) {
	constexpr double eps = 1e-5;
	constexpr double epsSq = eps * eps;
	if (glm::dot(forward, forward) < epsSq) {
		m_rotation = glm::dmat3(1.0); // zero-length forward vector, set rotation to identity
		return *this;
	}
	if (glm::dot(up, up) < epsSq) {
		m_rotation = glm::dmat3(1.0); // zero-length up vector, set rotation to identity
		return *this;
	}

	m_rotation[2] = -1.0 * (normalized ? forward : glm::normalize(forward));
	glm::dvec3 const& right = glm::cross(up, m_rotation[2]); // cross(Y, Z)
	m_rotation[0] = right * glm::inversesqrt(glm::max(eps, glm::dot(right, right)));
	m_rotation[1] = glm::cross(m_rotation[2], m_rotation[0]); // cross(Z, X)
	return *this;
}

Transform& Transform::setRotation(const glm::dquat& rotation, bool normalized) {
	m_rotation = (normalized) ? (glm::mat3_cast(rotation)) : (glm::mat3_cast(glm::normalize(rotation)));
	return *this;
}

Transform& Transform::setRotation(const glm::dvec3& eulerAngles) {
	setRotation(eulerAngles.x, eulerAngles.y, eulerAngles.z);
	return *this;
}

Transform& Transform::setRotation(double pitch, double yaw, double roll) {
	//setRotation(glm::yawPitchRoll(yaw, pitch, roll));
	setRotation(glm::eulerAngleYXZ(yaw, pitch, roll));
	return *this;
}

Transform& Transform::setRotation(double pitch, double yaw) {
	setRotation(pitch, yaw, 0.0);
	return *this;
}

Transform& Transform::setScale(const glm::dvec3& scale) {
	m_scale = scale;
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
	return *this;
}

Transform& Transform::translate(double x, double y) {
	m_translation.x += x;
	m_translation.y += y;
	return *this;
}

Transform& Transform::rotate(const glm::dquat& rotation, bool normalized) {
	normalized 
		? setRotation(rotation * getRotation()) 
		: setRotation(glm::normalize(rotation) * getRotation());
	return *this;
}

Transform& Transform::rotate(const glm::dmat3& rotation, bool normalized) {
	if (normalized) {
		m_rotation = rotation * m_rotation;
	} else {
		m_rotation = glm::dmat3(
			glm::normalize(m_rotation[0]),
			glm::normalize(m_rotation[1]),
			glm::normalize(m_rotation[2])
		) * m_rotation;
	}
	return *this;
}

Transform& Transform::rotate(const glm::dmat4& rotation, bool normalized) {
	rotate(glm::dmat3(rotation), normalized);
	return *this;
}

Transform& Transform::rotate(const glm::dvec3& axis, double angle) {
	m_rotation = glm::dmat3(glm::rotate(glm::dmat4(m_rotation), angle, axis));
	return *this;
}

Transform& Transform::rotate(const glm::dvec4& axisAngle) {
	rotate(glm::dvec3(axisAngle.x, axisAngle.y, axisAngle.z), axisAngle.w);
	return *this;
}

Transform& Transform::rotate(double x, double y, double z, double angle) {
	rotate(glm::dvec3(x, y, z), angle);
	return *this;
}

Transform& Transform::rotate(const glm::dvec3& eulerAngles) {
	setRotation(getEulerAngles() + eulerAngles);
	return *this;
}

Transform& Transform::rotate(double pitch, double yaw, double roll) {
	setRotation(getEulerAngles() + glm::dvec3(pitch, yaw, roll));
	return *this;
}

Transform& Transform::scale(const glm::dvec3& scale) {
	m_scale *= scale;
	return *this;
}

Transform& Transform::scale(double x, double y, double z) {
	m_scale.x *= x;
	m_scale.y *= y;
	m_scale.z *= z;
	return *this;
}

Transform& Transform::scale(double scale) {
	m_scale.x *= scale;
	m_scale.y *= scale;
	m_scale.z *= scale;
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
	return *this;
}

bool Transform::equalsTranslation(const Transform& other, double epsilon) const {
	if (!glm::epsilonEqual(m_translation.x, other.m_translation.x, epsilon)) return false;
	if (!glm::epsilonEqual(m_translation.y, other.m_translation.y, epsilon)) return false;
	if (!glm::epsilonEqual(m_translation.z, other.m_translation.z, epsilon)) return false;
	return true;
}

bool Transform::equalsRotation(const Transform& other, double epsilon) const {
	if (!glm::epsilonEqual(m_rotation[0].x, other.m_rotation[0].x, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[0].y, other.m_rotation[0].y, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[0].z, other.m_rotation[0].z, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[1].x, other.m_rotation[1].x, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[1].y, other.m_rotation[1].y, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[1].z, other.m_rotation[1].z, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[2].x, other.m_rotation[2].x, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[2].y, other.m_rotation[2].y, epsilon)) return false;
	if (!glm::epsilonEqual(m_rotation[2].z, other.m_rotation[2].z, epsilon)) return false;
	return true;
}

bool Transform::equalsScale(const Transform& other, double epsilon) const {
	if (!glm::epsilonEqual(m_scale.x, other.m_scale.x, epsilon)) return false;
	if (!glm::epsilonEqual(m_scale.y, other.m_scale.y, epsilon)) return false;
	if (!glm::epsilonEqual(m_scale.z, other.m_scale.z, epsilon)) return false;
	return true;
}

bool Transform::operator==(const Transform& other) const {
	if (!equalsTranslation(other, 1e-10)) return false;
	if (!equalsRotation(other, 1e-5)) return false;
	if (!equalsScale(other, 1e-10)) return false;
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
	return glm::dmat4(
		glm::dvec4(m_rotation[0] * m_scale.x, 0.0),
		glm::dvec4(m_rotation[1] * m_scale.y, 0.0),
		glm::dvec4(m_rotation[2] * m_scale.z, 0.0),
		glm::dvec4(m_translation, 1.0)
	);
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
	return *this;
}

Transform::operator glm::dmat4() const {
	return getMatrix();
}
