#include "core/engine/renderer/RenderCamera.h"
#include "core/graphics/Buffer.h"

RenderCamera::RenderCamera() :
        m_transform(),
        m_prevTransform(),
        m_projection(),
        m_prevProjection(),
        m_viewMatrix(1.0F),
        m_projectionMatrix(1.0F),
        m_viewProjectionMatrix(1.0F),
        m_inverseViewMatrix(1.0F),
        m_inverseProjectionMatrix(1.0F),
        m_inverseViewProjectionMatrix(1.0F),
        m_prevViewMatrix(m_viewMatrix),
        m_prevProjectionMatrix(m_projectionMatrix),
        m_prevViewProjectionMatrix(m_viewProjectionMatrix) {
}

RenderCamera::~RenderCamera() {
}

void RenderCamera::update() {
    m_prevViewMatrix = m_viewMatrix;
    m_prevProjectionMatrix = m_projectionMatrix;
    m_prevViewProjectionMatrix = m_viewProjectionMatrix;

    if (m_projection != m_prevProjection) {
        m_projectionMatrix = m_projection.getProjectionMatrix();
        //m_projectionMatrix = glm::scale(m_projectionMatrix, glm::vec3(1.0F, -1.0F, 1.0F));
        m_inverseProjectionMatrix = glm::inverse(m_projectionMatrix);
    }

    //if (m_transform != m_prevTransform) {
    m_inverseViewMatrix = m_transform.getMatrix();
    m_viewMatrix = glm::inverse(m_inverseViewMatrix);
    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
    m_inverseViewProjectionMatrix = glm::inverse(m_viewProjectionMatrix);
    //}

    m_prevProjection = m_projection;
    m_prevTransform = m_transform;
}

Transform& RenderCamera::transform() {
    return m_transform;
}

const Transform& RenderCamera::getTransform() const {
    return m_transform;
}

void RenderCamera::setTransform(const Transform& transform) {
    m_transform = transform;
}

Camera& RenderCamera::projection() {
    return m_projection;
}

const Camera& RenderCamera::getProjection() const {
    return m_projection;
}

void RenderCamera::setProjection(const Camera& camera) {
    m_projection = camera;
}

void RenderCamera::lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
    m_transform.setTranslation(eye.x, eye.y, eye.z);
    m_transform.setRotation(center - eye, up, false);
}

void RenderCamera::lookAt(float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
    lookAt(glm::vec3(eyeX, eyeY, eyeZ), glm::vec3(centerX, centerY, centerZ), glm::vec3(upX, upY, upZ));
}

void RenderCamera::lookAt(glm::vec3 center, glm::vec3 up) {
    m_transform.setRotation(glm::dvec3(center) - m_transform.getTranslation(), up, false);
}

void RenderCamera::lookAt(float centerX, float centerY, float centerZ, float upX, float upY, float upZ) {
    lookAt(glm::vec3(centerX, centerY, centerZ), glm::vec3(upX, upY, upZ));
}

const glm::mat4& RenderCamera::getViewMatrix() const {
    return m_viewMatrix;
}

const glm::mat4& RenderCamera::getProjectionMatrix() const {
    return m_projectionMatrix;
}

const glm::mat4& RenderCamera::getViewProjectionMatrix() const {
    return m_viewProjectionMatrix;
}

const glm::mat4& RenderCamera::getInverseViewMatrix() const {
    return m_inverseViewMatrix;
}

const glm::mat4& RenderCamera::getInverseProjectionMatrix() const {
    return m_inverseProjectionMatrix;
}

const glm::mat4& RenderCamera::getInverseViewProjectionMatrix() const {
    return m_inverseViewProjectionMatrix;
}

size_t RenderCamera::uploadCameraData(Buffer* buffer, const size_t& offset) const {
    CameraInfoUBO cameraInfoUBO{};
    cameraInfoUBO.viewMatrix = m_viewMatrix;
    cameraInfoUBO.projectionMatrix = m_projectionMatrix;
    cameraInfoUBO.viewProjectionMatrix = m_viewProjectionMatrix;
    buffer->upload(offset, sizeof(CameraInfoUBO), &cameraInfoUBO);
    return offset + sizeof(CameraInfoUBO);
}