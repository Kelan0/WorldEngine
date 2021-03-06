#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#define HAS_ALBEDO_TEXTURE_FLAG (1 << 0)
#define HAS_ROUGHNESS_TEXTURE_FLAG (1 << 1)
#define HAS_METALLIC_TEXTURE_FLAG (1 << 2)
#define HAS_EMISSION_TEXTURE_FLAG (1 << 3)
#define HAS_NORMAL_TEXTURE_FLAG (1 << 4)

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

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec3 fs_tangent;
layout(location = 2) in vec3 fs_bitangent;
layout(location = 3) in vec2 fs_texture;
layout(location = 4) in flat uint fs_objectIndex;

layout(location = 0) out vec4 out_AlbedoRGB_Roughness;
layout(location = 1) out vec4 out_NormalXYZ_Metallic;
layout(location = 2) out vec4 outEmissionRGB_AO;

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(set = 2, binding = 1) readonly buffer MaterialDataBuffer {
    Material objectMaterials[];
};

vec3 unpackRGB(in uint rgb) {
    const float scale = 1.0 / 255.0;
    return vec3(((rgb >>16) & 255) * scale,
                ((rgb >> 8) & 255) * scale,
                ((rgb) & 255) * scale);
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
        return ((material.packedRoughnessMetallicEmissionR) & 255) * scale;
    }
}

float getMaterialMetallic(in vec2 textureCoord, in Material material) {
    const float scale = 1.0 / 255.0;
    if (bool(material.flags & HAS_METALLIC_TEXTURE_FLAG)) {
        return texture(textures[material.metallicTextureIndex], textureCoord).r;
    } else {
        return ((material.packedRoughnessMetallicEmissionR >> 8) & 255) * scale;
    }
}

vec3 getMaterialEmission(in vec2 textureCoord, in Material material) {
    const float scale = 1.0 / 65535.0;
    if (bool(material.flags & HAS_EMISSION_TEXTURE_FLAG)) {
        return texture(textures[material.emissionTextureIndex], textureCoord).rgb;
    } else {
        return vec3(((material.packedRoughnessMetallicEmissionR >> 16) & 65535) * scale,
                    ((material.packedEmissionGB) & 65535) * scale,
                    ((material.packedEmissionGB >> 16) & 65535) * scale);
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
    Material material = objectMaterials[fs_objectIndex];

    out_AlbedoRGB_Roughness.rgb = getMaterialAlbedo(fs_texture, material);
    out_AlbedoRGB_Roughness.w = getMaterialRoughness(fs_texture, material);
    out_NormalXYZ_Metallic.xyz = getMaterialNormal(fs_texture, material);
    out_NormalXYZ_Metallic.w = getMaterialMetallic(fs_texture, material);
    outEmissionRGB_AO.rgb = getMaterialEmission(fs_texture, material);
    outEmissionRGB_AO.a = 1.0;
}