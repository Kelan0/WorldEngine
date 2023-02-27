
#include "core/engine/renderer/LightComponent.h"

LightComponent::LightComponent():
    m_type(LightType_Invalid),
    m_intensity(0.0F, 0.0F, 0.0F),
    m_shadowResolution(512, 512),
    m_flags_shadowCaster(false),
    m_flags_csmMapBasedSelection(true),
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

LightComponent& LightComponent::setAngularSize(const float& angularSize) {
    m_angularSize = angularSize;
    return *this;
}

LightComponent& LightComponent::setShadowCaster(const bool& shadowCaster) {
    m_flags_shadowCaster = shadowCaster;
    return *this;
}

LightComponent& LightComponent::setCsmMapBasedSelection(const bool& csmMapBasedSelection) {
    m_flags_csmMapBasedSelection = csmMapBasedSelection;
    return *this;
}

LightComponent& LightComponent::setShadowResolution(const glm::uvec2& shadowResolution) {
    m_shadowResolution = glm::min(shadowResolution, 4096u); // This should be a reasonable maximum resolution
    return *this;
}

LightComponent& LightComponent::setShadowResolution(const uint32_t& shadowWidth, const uint32_t& shadowHeight) {
    return setShadowResolution(glm::uvec2(shadowWidth, shadowHeight));
}

LightComponent& LightComponent::setShadowCascadeDistances(const std::vector<double>& cascadeDistances) {
    m_cascadeDistances = cascadeDistances;
    return *this;
}

const LightType& LightComponent::getType() const {
    return m_type;
}

const glm::vec3& LightComponent::getIntensity() const {
    return m_intensity;
}

const float& LightComponent::getAngularSize() const {
    return m_angularSize;
}

bool LightComponent::isShadowCaster() const {
    return m_flags_shadowCaster;
}

bool LightComponent::isCsmMapBasedSelection() const {
    return m_flags_csmMapBasedSelection;
}

const glm::uvec2& LightComponent::getShadowResolution() const {
    return m_shadowResolution;
}

const std::vector<double>& LightComponent::getShadowCascadeDistances() const {
    return m_cascadeDistances;
}

ShadowMap* LightComponent::getShadowMap() const {
    return m_shadowMap;
}

void LightComponent::setShadowMap(ShadowMap* shadowMap) {
    m_shadowMap = shadowMap;
}