
#include "core/engine/renderer/LightComponent.h"

LightComponent::LightComponent():
    m_type(LightType_Invalid),
    m_intensity(0.0F, 0.0F, 0.0F),
    m_shadowCaster(false),
    m_lightIndex(UINT32_MAX) {
}

LightComponent::~LightComponent() {

}

const LightType& LightComponent::getType() const {
    return m_type;
}

LightComponent& LightComponent::setType(const LightType& type) {
    m_type = type;
    return *this;
}

const glm::vec3& LightComponent::getIntensity() const {
    return m_intensity;
}

LightComponent& LightComponent::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
    return *this;
}

LightComponent& LightComponent::setIntensity(const float& r, const float& g, const float& b) {
    return setIntensity(glm::vec3(r, g, b));
}

const bool& LightComponent::isShadowCaster() const {
    return m_shadowCaster;
}

LightComponent& LightComponent::setShadowCaster(const bool& shadowCaster) {
    m_shadowCaster = shadowCaster;
    return *this;
}

const uint32_t& LightComponent::getLightIndex() const {
    return m_lightIndex;
}