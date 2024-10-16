#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"
#include "shaders/common/common.glsl"

#define HAS_ALBEDO_TEXTURE_FLAG uint(1 << 0)
#define HAS_ROUGHNESS_TEXTURE_FLAG uint(1 << 1)
#define HAS_METALLIC_TEXTURE_FLAG uint(1 << 2)
#define HAS_EMISSION_TEXTURE_FLAG uint(1 << 3)
#define HAS_NORMAL_TEXTURE_FLAG uint(1 << 4)

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec3 fs_tangent;
layout(location = 2) in vec3 fs_bitangent;
layout(location = 3) in vec2 fs_texture;
layout(location = 4) in vec4 fs_prevPosition;
layout(location = 5) in vec4 fs_currPosition;
layout(location = 6) in flat uint fs_materialIndex;


layout(location = 0) out vec4 out_AlbedoRGB_Roughness;
layout(location = 1) out vec4 out_NormalXYZ_Metallic;
layout(location = 2) out vec4 outEmissionRGB_AO;
layout(location = 3) out vec2 outVelocityXY;

layout(std140, set = 0, binding = 0) uniform UBO1 {
    CameraData prevCamera;
    CameraData camera;
    vec2 taaPreviousJitterOffset;
    vec2 taaCurrentJitterOffset;
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(set = 2, binding = 1) readonly buffer MaterialDataBuffer {
    Material objectMaterials[];
};

vec3 unpackRGB(in uint rgb) {
    const float scale = 1.0 / 255.0;
    return vec3(((rgb >> uint(16)) & uint(255)) * scale,
                ((rgb >> uint(8)) & uint(255)) * scale,
                ((rgb) & uint(255)) * scale);
}

vec3 getMaterialAlbedo(in vec2 textureCoord, in Material material) {
    if (bool(material.flags & HAS_ALBEDO_TEXTURE_FLAG)) {
        return texture(textures[material.albedoTextureIndex], textureCoord).rgb;
    } else {
        return unpackRGB(material.packedAlbedoColour);
    }
}

float getMaterialRoughness(in vec2 textureCoord, in Material material) {
    const float scale = 1.0 / 255.0;
    if (bool(material.flags & HAS_ROUGHNESS_TEXTURE_FLAG)) {
        return texture(textures[material.roughnessTextureIndex], textureCoord).r;
    } else {
        return ((material.packedRoughnessMetallicEmissionR) & uint(255)) * scale;
    }
}

float getMaterialMetallic(in vec2 textureCoord, in Material material) {
    const float scale = 1.0 / 255.0;
    if (bool(material.flags & HAS_METALLIC_TEXTURE_FLAG)) {
        return texture(textures[material.metallicTextureIndex], textureCoord).r;
    } else {
        return ((material.packedRoughnessMetallicEmissionR >> uint(8)) & uint(255)) * scale;
    }
}

vec3 getMaterialEmission(in vec2 textureCoord, in Material material) {
    const float scale = 1.0 / 65535.0;
    if (bool(material.flags & HAS_EMISSION_TEXTURE_FLAG)) {
        return texture(textures[material.emissionTextureIndex], textureCoord).rgb;
    } else {
        return vec3(((material.packedRoughnessMetallicEmissionR >> uint(16)) & uint(65535)) * scale,
                    ((material.packedEmissionGB) & uint(65535)) * scale,
                    ((material.packedEmissionGB >> uint(16)) & uint(65535)) * scale);
    }
}

vec3 getMaterialNormal(in vec2 textureCoord, in Material material) {
    if (bool(material.flags & HAS_NORMAL_TEXTURE_FLAG)) {
        mat3 TBN = mat3(fs_tangent, fs_bitangent, fs_normal);
        vec3 tangentSpaceNormal = texture(textures[material.normalTextureIndex], textureCoord).xyz * 2.0 - 1.0;
        return normalize(TBN * tangentSpaceNormal);
    } else {
        return normalize(fs_normal);
    }
}

void main() {
    Material material = objectMaterials[fs_materialIndex];

    out_AlbedoRGB_Roughness.rgb = getMaterialAlbedo(fs_texture, material);
    out_AlbedoRGB_Roughness.w = getMaterialRoughness(fs_texture, material);
    out_NormalXYZ_Metallic.xyz = getMaterialNormal(fs_texture, material);
    out_NormalXYZ_Metallic.w = getMaterialMetallic(fs_texture, material);
    outEmissionRGB_AO.rgb = getMaterialEmission(fs_texture, material);
    outEmissionRGB_AO.a = 1.0;

//    out_AlbedoRGB_Roughness = vec4(0.0);
//    outEmissionRGB_AO.rgb = texture(textures[material.normalTextureIndex], fs_texture).xyz;

    vec2 currPositionNDC = (fs_currPosition.xy / fs_currPosition.w);
    vec2 prevPositionNDC = (fs_prevPosition.xy / fs_prevPosition.w);
    vec2 currPosition = (currPositionNDC.xy - taaCurrentJitterOffset.xy);
    vec2 prevPosition = (prevPositionNDC.xy - taaPreviousJitterOffset.xy);
    vec2 velocity = (currPosition.xy - prevPosition.xy) * vec2(0.5, -0.5);

    outVelocityXY.xy = velocity * VELOCITY_PRECISION_SCALE; // Scale velocity to maintain precision. It needs to be divided again when accessed
}