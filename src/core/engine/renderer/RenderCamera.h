#pragma once

#include "../../core.h"

class RenderCamera {
public:
	RenderCamera();

	~RenderCamera();

	void update();

	glm::vec3 getPosition() const;

	glm::vec3 getAxisX() const;
	
	glm::vec3 getAxisY() const;

	glm::vec3 getAxisZ() const;

	glm::mat3 getRotationMatrix() const;

	glm::quat getRotation() const;

	void setPosition(glm::vec3 position);

	void setPosition(float x, float y, float z);

	void setRotation(glm::quat rotation);

	void setRotation(glm::vec3 z, glm::vec3 y);

	void lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up);

	void lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);

	void lookAt(glm::vec3 center, glm::vec3 up);

	void lookAt(float centerX, float centerY, float centerZ, float upX, float upY, float upZ);

	const glm::mat4& getViewMatrix() const;

	const glm::mat4& getProjectionMatrix() const;

	const glm::mat4& getViewProjectionMatrix() const;

	const glm::mat4& getInverseViewMatrix() const;

	const glm::mat4& getInverseProjectionMatrix() const;

	const glm::mat4& getInverseViewProjectionMatrix() const;

	float getFovRadians() const;

	float getFovDegrees() const;

	float getAspect() const;

	float getNearPlane() const;

	float getFarPlane() const;

	void setPerspective(float fov, float aspect, float near, float far);

	void setFovRadians(float fov);

	void setFovDegrees(float fov);

	void setAspect(float aspect);

	void setClipppingPlanes(float near, float far);


private:
	glm::vec3 m_position;
	glm::quat m_rotation;
	glm::vec3 m_prevPosition;
	glm::quat m_prevRotation;

	float m_fov;
	float m_aspect;
	float m_near;
	float m_far;

	glm::mat4 m_viewMatrix;
	glm::mat4 m_projectionMatrix;
	glm::mat4 m_viewProjectionMatrix;
	glm::mat4 m_inverseViewMatrix;
	glm::mat4 m_inverseProjectionMatrix;
	glm::mat4 m_inverseViewProjectionMatrix;

	glm::mat4 m_prevViewMatrix;
	glm::mat4 m_prevProjectionMatrix;
	glm::mat4 m_prevViewProjectionMatrix;

	bool m_viewChanged;
	bool m_projectionChanged;
};

