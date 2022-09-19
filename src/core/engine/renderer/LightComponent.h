
#ifndef WORLDENGINE_LIGHTCOMPONENT_H
#define WORLDENGINE_LIGHTCOMPONENT_H

#include "core/core.h"
#include "core/engine/renderer/RenderLight.h"

class ShadowMap;

class LightComponent {
    friend class LightRenderer;
public:
    LightComponent();

    ~LightComponent();

    LightComponent& setType(const LightType& type);

    LightComponent& setIntensity(const glm::vec3& intensity);

    LightComponent& setIntensity(const float& r, const float& g, const float& b);

    LightComponent& setShadowCaster(const bool& shadowCaster);

    LightComponent& setShadowResolution(const glm::uvec2& shadowResolution);

    LightComponent& setShadowResolution(const uint32_t& shadowWidth, const uint32_t& shadowHeight);

    [[nodiscard]] const LightType& getType() const;

    [[nodiscard]] const glm::vec3& getIntensity() const;

    [[nodiscard]] const bool& isShadowCaster() const;

    [[nodiscard]] const glm::uvec2& getShadowResolution() const;

    [[nodiscard]] ShadowMap* getShadowMap() const;

private:
    void setShadowMap(ShadowMap* shadowMap);

private:
    LightType m_type;
    glm::vec3 m_intensity;
    bool m_shadowCaster;
    glm::uvec2 m_shadowResolution;
    ShadowMap* m_shadowMap;
};


#endif //WORLDENGINE_LIGHTCOMPONENT_H
