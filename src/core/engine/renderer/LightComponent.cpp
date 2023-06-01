
#include "core/engine/renderer/LightComponent.h"
#include "core/engine/renderer/ShadowMap.h"

LightComponent::LightComponent():
    m_type(LightType_Invalid),
    m_intensity(0.0F, 0.0F, 0.0F),
    m_angularSize(glm::radians(0.52F)), // Angular size of sun
    m_shadowResolution(512, 512),
    m_flags_shadowCaster(false),
    m_flags_csmMapBasedSelection(true),
    m_shadowMap(nullptr) {
}

LightComponent::~LightComponent() = default;

LightComponent& LightComponent::setType(LightType type) {
    m_type = type;
    return *this;
}

LightComponent& LightComponent::setIntensity(const glm::vec3& intensity) {
    m_intensity = intensity;
    return *this;
}

LightComponent& LightComponent::setIntensity(float r, float g, float b) {
    return setIntensity(glm::vec3(r, g, b));
}

LightComponent& LightComponent::setAngularSize(float angularSize) {
    m_angularSize = angularSize;
    return *this;
}

LightComponent& LightComponent::setShadowCaster(bool shadowCaster) {
    m_flags_shadowCaster = shadowCaster;
    return *this;
}

LightComponent& LightComponent::setCsmMapBasedSelection(bool csmMapBasedSelection) {
    m_flags_csmMapBasedSelection = csmMapBasedSelection;
    return *this;
}

LightComponent& LightComponent::setShadowResolution(const glm::uvec2& shadowResolution) {
    m_shadowResolution = glm::min(shadowResolution, 4096u); // This should be a reasonable maximum resolution
    return *this;
}

LightComponent& LightComponent::setShadowResolution(uint32_t shadowWidth, uint32_t shadowHeight) {
    return setShadowResolution(glm::uvec2(shadowWidth, shadowHeight));
}

LightComponent& LightComponent::setShadowCascadeDistances(const std::vector<double>& cascadeDistances) {
    m_cascadeDistances = cascadeDistances;
    return *this;
}

LightType LightComponent::getType() const {
    return m_type;
}

const glm::vec3& LightComponent::getIntensity() const {
    return m_intensity;
}

float LightComponent::getAngularSize() const {
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