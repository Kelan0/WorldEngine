
#ifndef WORLDENGINE_LIGHTCOMPONENT_H
#define WORLDENGINE_LIGHTCOMPONENT_H

#include "core/core.h"
#include "core/engine/renderer/RenderLight.h"
#include "core/engine/renderer/ShadowMap.h"

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

    LightComponent& setCsmMapBasedSelection(const bool& csmMapBasedSelection);

    LightComponent& setShadowResolution(const glm::uvec2& shadowResolution);

    LightComponent& setShadowResolution(const uint32_t& shadowWidth, const uint32_t& shadowHeight);

    LightComponent& setShadowCascadeDistances(const std::vector<double>& cascadeDistances);

    [[nodiscard]] const LightType& getType() const;

    [[nodiscard]] const glm::vec3& getIntensity() const;

    [[nodiscard]] bool isShadowCaster() const;

    [[nodiscard]] bool isCsmMapBasedSelection() const;

    [[nodiscard]] const glm::uvec2& getShadowResolution() const;

    [[nodiscard]] const std::vector<double>& getShadowCascadeDistances() const;

    [[nodiscard]] ShadowMap* getShadowMap() const;

private:
    void setShadowMap(ShadowMap* shadowMap);

private:
    LightType m_type;
    glm::vec3 m_intensity;
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
