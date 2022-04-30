#version 450

#extension GL_EXT_nonuniform_qualifier : enable


layout(location = 0) in vec3 fs_normal;
layout(location = 1) in vec2 fs_texture;
layout(location = 2) in flat uint fs_textureIndex;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;

layout(set = 2, binding = 0) uniform sampler2D textures[];

void main() {
    vec3 albedo = texture(textures[(fs_textureIndex)], fs_texture).rgb;

    outColor = vec4(albedo, 1.0);
    outNormal = vec4(normalize(fs_normal), 0.0);
}