#version 450

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D testTexture;

void main() {
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);
    vec3 L = normalize(vec3(1, 1, 1));
    vec3 N = normalize(fs_normal);
    float NdotL = dot(N, L);

    vec3 albedo = texture(testTexture, fs_texture).rgb;

    vec3 surfaceColour = NdotL * albedo;
    outColor = vec4(surfaceColour, 1.0);

    //outColor = vec4(fs_texture, 0.0, 1.0);
}