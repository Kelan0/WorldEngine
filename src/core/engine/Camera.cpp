#include "Camera.h"

Camera::Camera() :
	m_position(0.0F),
	m_rotation(1.0F, 0.0F, 0.0F, 0.0F),
	m_prevPosition(m_position),
	m_prevRotation(m_rotation),
	m_fov(glm::radians(90.0F)),
	m_aspect(1.0F),
	m_near(0.1F),
	m_far(100.0F),
	m_viewMatrix(1.0F),
	m_projectionMatrix(1.0F),
	m_viewProjectionMatrix(1.0F),
	m_inverseViewMatrix(1.0F),
	m_inverseProjectionMatrix(1.0F),
	m_inverseViewProjectionMatrix(1.0F),
	m_prevViewMatrix(m_viewMatrix),
	m_prevProjectionMatrix(m_projectionMatrix),
	m_prevViewProjectionMatrix(m_viewProjectionMatrix),
	m_viewChanged(true),
	m_projectionChanged(true) {
}

Camera::~Camera() {
}

void Camera::update() {
	m_prevPosition = m_position;
	m_prevRotation = m_rotation;
	m_prevViewMatrix = m_viewMatrix;
	m_prevProjectionMatrix = m_projectionMatrix;
	m_prevViewProjectionMatrix = m_viewProjectionMatrix;

	if (m_projectionChanged) {
		m_projectionMatrix = glm::perspective(m_fov, m_aspect, m_near, m_far);
		m_inverseProjectionMatrix = glm::inverse(m_projectionMatrix);
	}
	if (m_viewChanged) {
		m_inverseViewMatrix = glm::mat4(getRotationMatrix());
		m_inverseViewMatrix[3] = glm::vec4(m_position, 1.0F);
		m_viewMatrix = glm::inverse(m_inverseViewMatrix);
	}
	if (m_viewChanged || m_projectionChanged) {
		m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
		m_inverseViewProjectionMatrix = glm::inverse(m_viewProjectionMatrix);
	}

	m_viewChanged = false;
	m_projectionChanged = false;
}

glm::vec3 Camera::getPosition() const {
	return m_position;
}

glm::vec3 Camera::getAxisX() const {
	float qyy = (m_rotation.y * m_rotation.y);
	float qzz = (m_rotation.z * m_rotation.z);
	float qxz = (m_rotation.x * m_rotation.z);
	float qxy = (m_rotation.x * m_rotation.y);
	float qwy = (m_rotation.w * m_rotation.y);
	float qwz = (m_rotation.w * m_rotation.z);

	return glm::vec3(1.0F - 2.0F * (qyy + qzz), 2.0F * (qxy + qwz), 2.0F * (qxz - qwy));
}

glm::vec3 Camera::getAxisY() const {
	float qxx = (m_rotation.x * m_rotation.x);
	float qzz = (m_rotation.z * m_rotation.z);
	float qxy = (m_rotation.x * m_rotation.y);
	float qyz = (m_rotation.y * m_rotation.z);
	float qwx = (m_rotation.w * m_rotation.x);
	float qwz = (m_rotation.w * m_rotation.z);

	return glm::vec3(2.0F * (qxy - qwz), 1.0F - 2.0F * (qxx + qzz), 2.0F * (qyz + qwx));
}

glm::vec3 Camera::getAxisZ() const {
	float qxx = (m_rotation.x * m_rotation.x);
	float qyy = (m_rotation.y * m_rotation.y);
	float qxz = (m_rotation.x * m_rotation.z);
	float qyz = (m_rotation.y * m_rotation.z);
	float qwx = (m_rotation.w * m_rotation.x);
	float qwy = (m_rotation.w * m_rotation.y);

	return glm::vec3(2.0F * (qxz + qwy), 2.0F * (qyz - qwx), 1.0F - 2.0F * (qxx + qyy));
}

glm::mat3 Camera::getRotationMatrix() const {
	return glm::mat3_cast(m_rotation);
}

glm::quat Camera::getRotation() const {
	return m_rotation;
}

void Camera::setPosition(glm::vec3 position) {
	if (position != m_position) {
		m_position = position;
		m_viewChanged = true;
	}
}

void Camera::setPosition(float x, float y, float z) {
	setPosition(glm::vec3(x, y, z));
}

void Camera::setRotation(glm::quat rotation) {
	if (rotation != m_rotation) { // TODO: threshold test?
		m_rotation = rotation;
		m_viewChanged = true;
	}
}

void Camera::setRotation(glm::vec3 z, glm::vec3 y) {
	constexpr float eps = 1e-5;
	constexpr float epsSq = eps * eps;

	float zLen = glm::length(z);
	if (zLen <= eps) {
		// z is zero-length, set rotation to identity quaternion
		setRotation(glm::quat(1.0F, 0.0F, 0.0F, 0.0F));
	} else {
		z /= zLen;

		float yLen = glm::dot(y, y);
		if (yLen <= epsSq) {
			// y is zero-length, default to global up (0, 1, 0)
			y = glm::vec3(0, 1, 0); 

		} else if (glm::abs(yLen - 1.0F) > eps) {
			// y is not normalized
			y = glm::normalize(y);
		}

		if (glm::abs(glm::dot(z, y)) > 0.999F) {
			// z and y point in the same direction, choose a different y-axis
			y = glm::cross(this->getAxisX(), z);
		}

		setRotation(glm::quatLookAt(z, y));
	}
}

void Camera::lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
	setPosition(eye);
	setRotation(center - eye, up);
}

void Camera::lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
	lookAt(glm::vec3(eyeX, eyeY, eyeZ), glm::vec3(centerX, centerY, centerZ), glm::vec3(upX, upY, upZ));
}

void Camera::lookAt(glm::vec3 center, glm::vec3 up) {
	setRotation(center - m_position, up);
}

void Camera::lookAt(float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
	lookAt(glm::vec3(centerX, centerY, centerZ), glm::vec3(upX, upY, upZ));
}

const glm::mat4& Camera::getViewMatrix() const {
	return m_viewMatrix;
}

const glm::mat4& Camera::getProjectionMatrix() const {
	return m_projectionMatrix;
}

const glm::mat4& Camera::getViewProjectionMatrix() const {
	return m_viewProjectionMatrix;
}

const glm::mat4& Camera::getInverseViewMatrix() const {
	return m_inverseViewMatrix;
}

const glm::mat4& Camera::getInverseProjectionMatrix() const {
	return m_inverseProjectionMatrix;
}

const glm::mat4& Camera::getInverseViewProjectionMatrix() const {
	return m_inverseViewProjectionMatrix;
}

float Camera::getFovRadians() const {
	return m_fov;
}

float Camera::getFovDegrees() const {
	return glm::degrees(m_fov);
}

float Camera::getAspect() const {
	return m_aspect;
}

float Camera::getNearPlane() const {
	return m_near;
}

float Camera::getFarPlane() const {
	return m_far;
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
	setFovRadians(fov);
	setAspect(aspect);
	setClipppingPlanes(near, far);
}

void Camera::setFovRadians(float fov) {
	if (fov != m_fov && !isnan(fov)) {
		m_fov = fov;
		m_projectionChanged = true;
	}
}

void Camera::setFovDegrees(float fov) {
	setFovRadians(glm::radians(fov));
}

void Camera::setAspect(float aspect) {
	if (aspect != m_aspect && !isnan(aspect)) {
		m_aspect = aspect;
		m_projectionChanged = true;
	}
}

void Camera::setClipppingPlanes(float near, float far) {
	if ((near != m_near || far != m_far) && !isnan(near) && !isnan(far)) {
		m_near = near;
		m_far = far;
		m_projectionChanged = true;
	}
}
