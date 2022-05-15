
#ifndef WORLDENGINE_LIGHTCOMPONENT_H
#define WORLDENGINE_LIGHTCOMPONENT_H

#include "core/core.h"
#include "core/engine/renderer/RenderLight.h"

class LightComponent {
public:
    LightComponent();

    ~LightComponent();

    const LightType& getType() const;

    LightComponent& setType(const LightType& type);

    const glm::vec3& getIntensity() const;

    LightComponent& setIntensity(const glm::vec3& intensity);

    LightComponent& setIntensity(const float& r, const float& g, const float& b);

    const bool& isShadowCaster() const;

    LightComponent& setShadowCaster(const bool& shadowCaster);

    const uint32_t& getLightIndex() const;

private:
    LightType m_type;
    glm::vec3 m_intensity;
    bool m_shadowCaster;
    uint32_t m_lightIndex;
};


#endif //WORLDENGINE_LIGHTCOMPONENT_H
