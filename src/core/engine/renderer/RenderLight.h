
#ifndef WORLDENGINE_RENDERLIGHT_H
#define WORLDENGINE_RENDERLIGHT_H

#include "core/core.h"

enum LightType {
    LightType_Invalid = 0,
    LightType_Directional = 1,
    LightType_Point = 2,
    LightType_Spot = 3,
    LightType_Area = 4,
};

struct GPULight {
    glm::vec4 worldPosition;
    glm::vec4 worldDirection;
    glm::vec4 intensity;
    glm::vec3 _pad1;
    float cosAngularSize;
    uint32_t shadowMapIndex;
    uint32_t shadowMapCount; // Number of cascades for CSM directional lights
    uint32_t type;
    union {
        uint32_t flags;
        struct {
            bool flags_csmMapBasedSelection : 1;
        };
    };
};

class Light {
public:

public:
    Light(const LightType& type);

    virtual ~Light() = default;

    const LightType& getType() const;

    virtual void copyLightData(GPULight* dst) const = 0;

private:
    LightType m_type;
};

class DirectionalLight : public Light {
public:
    DirectionalLight();

    const glm::vec3& getDirection() const;

    void setDirection(const glm::vec3& direction);

    const glm::vec3& getIntensity() const;

    void setIntensity(const glm::vec3& intensity);

    const float& getAngularSize() const;

    void setAngularSize(const float& angularSize);

    virtual void copyLightData(GPULight* dst) const override;

private:
    glm::vec3 m_direction;
    glm::vec3 m_intensity;
    float m_angularSize; // Angular diameter in radians
};

class PointLight : public Light {
public:
    PointLight();

    const glm::dvec3& getPosition() const;

    void setPosition(const glm::dvec3& position);

    const glm::vec3& getIntensity() const;

    void setIntensity(const glm::vec3& intensity);

    virtual void copyLightData(GPULight* dst) const override;

private:
    glm::dvec3 m_position;
    glm::vec3 m_intensity;
};




#endif //WORLDENGINE_RENDERLIGHT_H
