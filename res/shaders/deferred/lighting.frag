#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    vec4 temp;
};

layout(set = 0, binding = 1) uniform sampler2D texture_AlbedoRGB_Roughness;
layout(set = 0, binding = 2) uniform sampler2D texture_NormalXYZ_Metallic;
layout(set = 0, binding = 3) uniform sampler2D texture_Depth;


void main() {
    vec4 texelValue;
    
    texelValue = texture(texture_AlbedoRGB_Roughness, fs_texture);
    vec3 albedo = texelValue.rgb;
    float roughness = texelValue.w;
    texelValue = texture(texture_NormalXYZ_Metallic, fs_texture);
    vec3 normal = texelValue.xyz;
    float metallic = texelValue.w;

    float depth = texture(texture_Depth, fs_texture).r;

    if (dot(normal, normal) < 1e-8) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 L = normalize(vec3(1, 1, 2));
    vec3 N = normal;
    float NdotL = dot(N, L);
    vec3 surfaceColour = NdotL * albedo;

    outColor = vec4(surfaceColour, 1.0);
}