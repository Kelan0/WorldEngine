#pragma once

#include "../../core.h"

class Camera {
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

	Camera& operator=(const Camera& other);

	bool operator==(const Camera& other) const;

	bool operator!=(const Camera& other) const;

	operator glm::mat4() const;
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

