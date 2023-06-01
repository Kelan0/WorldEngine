
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

    LightComponent& setType(LightType type);

    LightComponent& setIntensity(const glm::vec3& intensity);

    LightComponent& setIntensity(float r, float g, float b);

    LightComponent& setAngularSize(float angularSize);

    LightComponent& setShadowCaster(bool shadowCaster);

    LightComponent& setCsmMapBasedSelection(bool csmMapBasedSelection);

    LightComponent& setShadowResolution(const glm::uvec2& shadowResolution);

    LightComponent& setShadowResolution(uint32_t shadowWidth, uint32_t shadowHeight);

    LightComponent& setShadowCascadeDistances(const std::vector<double>& cascadeDistances);

    LightType getType() const;

    const glm::vec3& getIntensity() const;

    float getAngularSize() const;

    bool isShadowCaster() const;

    bool isCsmMapBasedSelection() const;

    const glm::uvec2& getShadowResolution() const;

    const std::vector<double>& getShadowCascadeDistances() const;

    ShadowMap* getShadowMap() const;

private:
    void setShadowMap(ShadowMap* shadowMap);

private:
    LightType m_type;
    glm::vec3 m_intensity;
    float m_angularSize; // angular size in radians
    glm::uvec2 m_shadowResolution;
    ShadowMap* m_shadowMap;
    std::vector<double> m_cascadeDistances;
    union {
        uint32_t m_flags;
        struct {
            bool m_flags_shadowCaster : 1;
            bool m_flags_csmMapBasedSelection : 1;
        };
    };
};


#endif //WORLDENGINE_LIGHTCOMPONENT_H
