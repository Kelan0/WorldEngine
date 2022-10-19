#ifndef _STRUCTURES_GLSL
#define _STRUCTURES_GLSL

struct ObjectData {
    mat4 prevModelMatrix;
    mat4 modelMatrix;
};

struct CameraData {
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 viewProjectionMatrix;
};

struct Material {
    uint albedoTextureIndex;
    uint roughnessTextureIndex;
    uint metallicTextureIndex;
    uint emissionTextureIndex;
    uint normalTextureIndex;
    uint packedAlbedoColour;
    uint packedRoughnessMetallicEmissionR;
    uint packedEmissionGB;
    uint flags;
    uint _pad0;
    uint _pad2;
    uint _pad3;
};

struct LightInfo {
    vec4 worldPosition;
    vec4 worldDirection;
    vec4 intensity;
    vec4 _pad1;
    uint shadowMapIndex;
    uint shadowMapCount;
    uint type;
    uint flags;
};

struct ShadowMapInfo {
    mat4 viewProjectionMatrix;
    float cascadeStartZ;
    float cascadeEndZ;
    float _pad0;
    float _pad1;
};

#endif