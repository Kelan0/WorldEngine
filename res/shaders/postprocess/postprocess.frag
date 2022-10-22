#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 fs_texture;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO1 {
    int temp;
};

layout(set = 0, binding = 1) uniform sampler2D frameTexture;

void main() {
    vec3 finalColour = texture(frameTexture, fs_texture).rgb;
    outColor = vec4(finalColour, 1.0);
}