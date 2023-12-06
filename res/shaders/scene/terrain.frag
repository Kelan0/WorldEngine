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
    uvec4 heightmapTextureIndex;
};

layout(set = 1, binding = 2) uniform sampler2D heightmapTextures[];

void main() {

    ivec2 texSize = textureSize(heightmapTextures[fs_heightmapTextureIndex], 0).xy;
    const float elevationScale = terrainScale.z;
    const ivec3 off = ivec3(1, 1, 0);
    float h = texture(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord).r;
    float hL = textureOffset(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord, ivec2(-1, 0)).r;
    float hR = textureOffset(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord, ivec2(+1, 0)).r;
    float hD = textureOffset(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord, ivec2(0, -1)).r;
    float hU = textureOffset(heightmapTextures[fs_heightmapTextureIndex], fs_textureCoord, ivec2(0, +1)).r;
//    hL = (floor(hL * 100.0) / 100.0);
//    hR = (floor(hR * 100.0) / 100.0);
//    hD = (floor(hD * 100.0) / 100.0);
//    hU = (floor(hU * 100.0) / 100.0);
    hL *= elevationScale;
    hR *= elevationScale;
    hD *= elevationScale;
    hU *= elevationScale;
    vec3 N;
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
//    out_AlbedoRGB_Roughness.rgb = vec3(fs_normal * 0.5 + 0.5);
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