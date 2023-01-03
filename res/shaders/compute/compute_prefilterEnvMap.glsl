#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (local_size_x = 16) in;
layout (local_size_y = 16) in;
layout (local_size_z = 1) in;

#include "shaders/common/common.glsl"
#include "shaders/common/pbr.glsl"

layout(push_constant) uniform PC1 {
    uint srcSize;
    uint dstSize;
    uint mipLevel;
    uint numMipLevels;
};

layout(set = 0, binding = 0) uniform samplerCube srcEnvironmentMap;
layout(set = 0, binding = 1, rgba32f) uniform writeonly imageCube dstPrefilteredEnvironmentMap[];

void main() {
    const ivec3 invocation = ivec3(gl_GlobalInvocationID.xyz);

    if (invocation.x >= dstSize || invocation.y >= dstSize)
        return;

    const int face = int(invocation.z);

    const float roughness = float(mipLevel) / float(numMipLevels);

    const vec2 textureCoord = (vec2(invocation.xy + 0.5)) / vec2(dstSize);

    const vec3 N = getCubemapRay(textureCoord, face);
    const vec3 R = N;
    const vec3 V = R;
    
    const float roughnessSquared = roughness * roughness;
    const int SAMPLE_COUNT = 1024;

    const float saTexel = 4.0 * PI / (6.0 * srcSize * srcSize);

    float totalWeight = 0.0;
    vec3 prefilteredColor = vec3(0.0);

    for(int i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H = importanceSampleGGX(Xi, N, roughnessSquared);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = dot(N, L);
        
        if(NdotL > 0.0) {
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);

            float D = distributionGGX(NdotH, roughness);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 

            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
            prefilteredColor += textureLod(srcEnvironmentMap, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor /= totalWeight;

    const vec3 dstCoord = getCubemapCoordinate(N) * vec3(dstSize, dstSize, 1.0);
    imageStore(dstPrefilteredEnvironmentMap[mipLevel], ivec3(invocation), vec4(prefilteredColor, 1.0));
}