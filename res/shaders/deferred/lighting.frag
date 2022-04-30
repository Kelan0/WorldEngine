#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    vec4 temp;
};

layout(set = 0, binding = 1) uniform sampler2D albedoTexture;
layout(set = 0, binding = 2) uniform sampler2D normalTexture;
layout(set = 0, binding = 3) uniform sampler2D depthTexture;


void main() {
    vec3 albedo = texture(albedoTexture, fs_texture).rgb;
    vec3 normal = texture(normalTexture, fs_texture).xyz;
    float depth = texture(depthTexture, fs_texture).r;

    if (dot(normal, normal) < 1e-8) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec3 L = normalize(vec3(1, 1, 1));
    vec3 N = normal;
    float NdotL = dot(N, L);
    vec3 surfaceColour = NdotL * albedo;

    outColor = vec4(vec3(surfaceColour), 1.0);
}