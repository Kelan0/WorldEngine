#pragma once

#include "../../core.h"

class Transform {
public:
	Transform();

	Transform(const glm::mat4& transformationMatrix);

	Transform(const Transform& copy);

	Transform(Transform&& move);

	~Transform();

	glm::dvec3 getTranslation() const;
	double getX() const;
	double getY() const;
	double getZ() const;

	glm::dquat getRotation() const;
	glm::dmat3 getRotationMatrix() const;
	glm::dvec3 getXAxis() const;
	glm::dvec3 getLeftAxis() const;
	glm::dvec3 getRightAxis() const;
	glm::dvec3 getYAxis() const;
	glm::dvec3 getDownAxis() const;
	glm::dvec3 getUpAxis() const;
	glm::dvec3 getZAxis() const;
	glm::dvec3 getForwardAxis() const;
	glm::dvec3 getBackwardAxis() const;
	glm::dvec3 getEulerAngles() const;
	double getPitch() const;
	double getYaw() const;
	double getRoll() const;

	glm::dvec3 getScale() const;
	double getScaleX() const;
	double getScaleY() const;
	double getScaleZ() const;

	Transform& setTranslation(const glm::dvec4& translation);
	Transform& setTranslation(const glm::dvec3& translation);
	Transform& setTranslation(const glm::dvec2& translation);
	Transform& setTranslation(double x, double y, double z);
	Transform& setTranslation(double x, double y);

	Transform& setRotation(const glm::dmat3& rotation);
	Transform& setRotation(const glm::dmat4& rotation);
	Transform& setRotation(const glm::dvec3& forward, const glm::dvec3& up, bool normalized = true);
	Transform& setRotation(const glm::dquat& rotation, bool normalized = true);
	Transform& setRotation(const glm::dvec3& eulerAngles);
	Transform& setRotation(double pitch, double yaw, double roll);
	Transform& setRotation(double pitch, double yaw);

	Transform& setScale(const glm::dvec3& scale);
	Transform& setScale(double x, double y, double z);
	Transform& setScale(double scale);

	Transform& translate(const glm::dvec4& translation);
	Transform& translate(const glm::dvec3& translation);
	Transform& translate(const glm::dvec2& translation);
	Transform& translate(double x, double y, double z);
	Transform& translate(double x, double y);

	Transform& rotate(const glm::dquat& rotation, bool normalized = true);
	Transform& rotate(const glm::dmat3& rotation, bool normalized = true);
	Transform& rotate(const glm::dmat4& rotation, bool normalized = true);
	Transform& rotate(const glm::dvec3& axis, double angle);
	Transform& rotate(const glm::dvec4& axisAngle);
	Transform& rotate(double x, double y, double z, double angle);
	Transform& rotate(const glm::dvec3& eulerAngles);
	Transform& rotate(double pitch, double yaw, double roll);

	Transform& scale(const glm::dvec3& scale);
	Transform& scale(double x, double y, double z);
	Transform& scale(double scale);


	Transform operator*(const glm::dmat4& other) const;
	Transform operator*(const Transform& other) const;

	Transform& operator=(glm::dmat4 other);
	Transform& operator=(const Transform& other);

	glm::dmat4 getMatrix() const;

	Transform& setMatrix(const glm::dmat4& matrix);

	operator glm::dmat4() const;
private:
	glm::dvec3 m_translation;
	glm::dmat3 m_rotation;
	glm::vec3 m_scale;
};

