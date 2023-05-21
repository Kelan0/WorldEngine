#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"
#include "shaders/common/common.glsl"

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec3 fs_tangent;
layout(location = 2) in vec3 fs_bitangent;
layout(location = 3) in vec2 fs_texture;
layout(location = 4) in vec4 fs_prevPosition;
layout(location = 5) in vec4 fs_currPosition;


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

void main() {
    out_AlbedoRGB_Roughness.rgb = vec3(0.1, 0.6, 0.1);
    out_AlbedoRGB_Roughness.w = 1.0;
    out_NormalXYZ_Metallic.xyz = normalize(fs_normal);
    out_NormalXYZ_Metallic.w = 0.0;
    outEmissionRGB_AO.rgb = vec3(0.0);
    outEmissionRGB_AO.a = 1.0;

    vec2 currPositionNDC = (fs_currPosition.xy / fs_currPosition.w);
    vec2 prevPositionNDC = (fs_prevPosition.xy / fs_prevPosition.w);
    vec2 currPosition = (currPositionNDC.xy - taaCurrentJitterOffset.xy);
    vec2 prevPosition = (prevPositionNDC.xy - taaPreviousJitterOffset.xy);
    vec2 velocity = (currPosition.xy - prevPosition.xy) * vec2(0.5, -0.5);

    outVelocityXY.xy = velocity * VELOCITY_PRECISION_SCALE; // Scale velocity to maintain precision. It needs to be divided again when accessed
}