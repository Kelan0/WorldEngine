#version 450

layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D testTexture;

void main() {
    //outColor = vec4(1.0, 0.0, 0.0, 1.0);

    outColor = vec4(texture(testTexture, fs_texture).rgb, 1.0);

    //outColor = vec4(fs_texture, 0.0, 1.0);
}