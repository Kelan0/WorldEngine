#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"
#include "shaders/common/common.glsl"

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec3 fs_tangent;
layout(location = 2) in vec3 fs_bitangent;
layout(location = 3) in vec2 fs_textureCoord;
layout(location = 4) in vec4 fs_prevPosition;
layout(location = 5) in vec4 fs_currPosition;
layout(location = 6) in flat uint fs_heightmapTextureIndex;


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

layout(std140, set = 1, binding = 0) uniform UBO2 {
    mat4 terrainTransformMatrix;
    vec4 terrainScale;
    uvec4 heightmapTextureIndex_tileGridSize;
};

layout(set = 1, binding = 2) uniform sampler2D heightmapTextures[];

void main() {

    const float elevationScale = terrainScale.z;
    const ivec3 off = ivec3(1, 1, 0);

    vec3 N;
//    N = normalize(fs_normal);

//    float h = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord).r;
//    N.x = dFdx(h);
//    N.y = dFdy(h);
//    N.z = 2.0;

    vec2 texSize = vec2(textureSize(heightmapTextures[fs_heightmapTextureIndex], 0).xy);
    vec2 offset = 0.1 / texSize;

    float hL = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord + vec2(-offset.x, 0)).r;
    float hR = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord + vec2(+offset.x, 0)).r;
    float hD = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord + vec2(0, -offset.y)).r;
    float hU = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord + vec2(0, +offset.y)).r;
    hL *= elevationScale;
    hR *= elevationScale;
    hD *= elevationScale;
    hU *= elevationScale;

    N.x = hL - hR;
    N.y = hU - hD;
    N.z = 2.0;

    mat3 TBN = mat3(fs_tangent, fs_bitangent, fs_normal);
    N = TBN * N;

    N = normalize(N);


    out_AlbedoRGB_Roughness.rgb = vec3(0.1, 0.6, 0.1);
//    out_AlbedoRGB_Roughness.rgb = vec3(0.0, 0.0, 0.0);
//    out_AlbedoRGB_Roughness.rgb = vec3(fract(fs_textureCoord * texSize), 0.0);
//    out_AlbedoRGB_Roughness.rgb = vec3(fract(h * 100) / 100);

//    out_AlbedoRGB_Roughness.rgb = vec3(N * 0.5 + 0.5);
    out_AlbedoRGB_Roughness.w = 1.0;
    out_NormalXYZ_Metallic.xyz = N;
    out_NormalXYZ_Metallic.w = 0.0;
    outEmissionRGB_AO.rgb = vec3(0.0);
    outEmissionRGB_AO.a = 1.0;
    outVelocityXY.xy = vec2(0.0, 0.0);

//    vec2 currPositionNDC = (fs_currPosition.xy / fs_currPosition.w);
//    vec2 prevPositionNDC = (fs_prevPosition.xy / fs_prevPosition.w);
//    vec2 currPosition = (currPositionNDC.xy - taaCurrentJitterOffset.xy);
//    vec2 prevPosition = (prevPositionNDC.xy - taaPreviousJitterOffset.xy);
//    vec2 velocity = (currPosition.xy - prevPosition.xy) * vec2(0.5, -0.5);
//    outVelocityXY.xy = velocity * VELOCITY_PRECISION_SCALE; // Scale velocity to maintain precision. It needs to be divided again when accessed
}