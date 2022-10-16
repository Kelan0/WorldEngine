
#include "core/engine/renderer/LightComponent.h"
#include "core/engine/renderer/ShadowMap.h"

LightComponent::LightComponent():
    m_type(LightType_Invalid),
    m_intensity(0.0F, 0.0F, 0.0F),
    m_shadowResolution(512, 512),
    m_shadowCaster(false),
    m_shadowMap(nullptr) {
}

LightComponent::~LightComponent() = default;

LightComponent& LightComponent::setType(const LightType& type) {
    m_type = type;
    return *this;
}

LightComponent& LightComponent::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
    return *this;
}

LightComponent& LightComponent::setIntensity(const float& r, const float& g, const float& b) {
    return setIntensity(glm::vec3(r, g, b));
}

LightComponent& LightComponent::setShadowCaster(const bool& shadowCaster) {
    m_shadowCaster = shadowCaster;
    return *this;
}

LightComponent& LightComponent::setShadowResolution(const glm::uvec2& shadowResolution) {
    m_shadowResolution = glm::min(shadowResolution, 4096u); // This should be a reasonable maximum resolution
    return *this;
}

LightComponent& LightComponent::setShadowResolution(const uint32_t& shadowWidth, const uint32_t& shadowHeight) {
    return setShadowResolution(glm::uvec2(shadowWidth, shadowHeight));
}

const LightType& LightComponent::getType() const {
    return m_type;
}

const glm::vec3& LightComponent::getIntensity() const {
    return m_intensity;
}

const bool& LightComponent::isShadowCaster() const {
    return m_shadowCaster;
}

const glm::uvec2& LightComponent::getShadowResolution() const {
    return m_shadowResolution;
}

ShadowMap* LightComponent::getShadowMap() const {
    return m_shadowMap;
}

void LightComponent::setShadowMap(ShadowMap* shadowMap) {
    m_shadowMap = shadowMap;
}