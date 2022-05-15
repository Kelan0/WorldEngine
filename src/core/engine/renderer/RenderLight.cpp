
#include "core/engine/renderer/RenderLight.h"

Light::Light(const LightType& type):
    m_type(type) {
}

const LightType& Light::getType() const {
    return m_type;
}

DirectionalLight::DirectionalLight():
    Light(LightType_Directional) {
}

const glm::vec3& DirectionalLight::getDirection() const {
    return m_direction;
}

void DirectionalLight::setDirection(const glm::vec3& direction) {
    m_direction = direction;
}

const glm::vec3& DirectionalLight::getIntensity() const {
    return m_intensity;
}

void DirectionalLight::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
}

void DirectionalLight::copyLightData(GPULight* dst) const {
    dst->type = getType();
    dst->worldDirection = glm::vec4(m_direction, 0.0F);
    dst->intensity = glm::vec4(m_intensity, 1.0F);
}


PointLight::PointLight():
    Light(LightType_Point) {
}

const glm::dvec3& PointLight::getPosition() const {
    return m_position;
}

void PointLight::setPosition(const glm::dvec3& position) {
    m_position = position;
}

const glm::vec3& PointLight::getIntensity() const {
    return m_intensity;
}

void PointLight::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
}

void PointLight::copyLightData(GPULight* dst) const {
    dst->type = getType();
    dst->worldPosition = glm::vec4(m_position, 0.0F);
    dst->intensity = glm::vec4(m_intensity, 1.0F);
}