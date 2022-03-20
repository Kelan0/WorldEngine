
#ifndef WORLDENGINE_RENDERCAMERA_H
#define WORLDENGINE_RENDERCAMERA_H

#include "../../core.h"

#include "../scene/Transform.h"
#include "../scene/Camera.h"

class RenderCamera {
public:
    RenderCamera();

    ~RenderCamera();

    void update();

    Transform& transform();

    const Transform& getTransform() const;

    void setTransform(const Transform& transform);

    Camera& projection();

    const Camera& getProjection() const;

    void setProjection(const Camera& camera);

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


private:
    Transform m_transform;
    Transform m_prevTransform;

    Camera m_projection;
    Camera m_prevProjection;

    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewProjectionMatrix;
    glm::mat4 m_inverseViewMatrix;
    glm::mat4 m_inverseProjectionMatrix;
    glm::mat4 m_inverseViewProjectionMatrix;

    glm::mat4 m_prevViewMatrix;
    glm::mat4 m_prevProjectionMatrix;
    glm::mat4 m_prevViewProjectionMatrix;
};


#endif //WORLDENGINE_RENDERCAMERA_H
