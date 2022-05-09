#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "res/shaders/common/pbr.glsl"

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

#include "res/shaders/common/common.glsl"
#include "res/shaders/common/pbr.glsl"

layout(push_constant) uniform PC1 {
    uvec2 dstSize;
};

layout(set = 0, binding = 0, rgba16f) uniform writeonly image2D dstBRDFIntegrationMap;

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);

    if (invocation.x >= dstSize.x || invocation.y >= dstSize.y)
        return;

    const vec2 textureCoord = (vec2(invocation.xy) + 0.5) / vec2(dstSize);
    const float NdotV = textureCoord.x;
    const float roughness = textureCoord.y;
    const float roughnessSquared = roughness * roughness;

    float k = geometryRoughnessIBL(roughness);

    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float scale = 0.0;
    float bias = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    const int SAMPLE_COUNT = 1024;
    for(int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importanceSampleGGX(Xi, N, roughnessSquared);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if(NdotL > 0.0) {
            float G = geometrySmith(NdotV, NdotL, k);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            
            scale += (1.0 - Fc) * G_Vis;
            bias += Fc * G_Vis;
        }
    }

    scale /= float(SAMPLE_COUNT);
    bias /= float(SAMPLE_COUNT);

    imageStore(dstBRDFIntegrationMap, ivec2(invocation.xy), vec4(scale, bias, 0.0, 1.0));
}