#ifndef _PBR_GLSL
#define _PBR_GLSL

#include "res/shaders/common/common.glsl"

float radicalInverse_VdC(uint bits)  {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), radicalInverse_VdC(i));
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

float distributionGGX(in float NdotH, in float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float geometryRoughnessDirect(in float roughness) {
    float r = (roughness + 1.0);
    return (r * r) * 0.125;
}

float geometryRoughnessIBL(in float roughness) {
    float a = roughness;
    return (a * a) * 0.5;
}

float geometrySchlickGGX(in float NdotV, in float k) {
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float geometrySmith(in float NdotV, in float NdotL, in float k) {
    float ggx2 = geometrySchlickGGX(NdotV, k);
    float ggx1 = geometrySchlickGGX(NdotL, k);
    return ggx1 * ggx2;
}

vec3 importanceSampleGGX(in vec2 Xi, in vec3 N, in float roughnessSquared) {
    float a = roughnessSquared * roughnessSquared;
	
    float phi = 2.0 * PI * Xi.x;
    float cosPhi = cos(phi);
    float sinPhi = sin(phi);
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cosPhi * sinTheta;
    H.y = sinPhi * sinTheta;
    H.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}  

#endif