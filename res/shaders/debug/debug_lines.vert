#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texture;
layout(location = 3) in vec4 colour;

layout(location = 0) out vec3 fs_normal;
layout(location = 1) out vec2 fs_texture;
layout(location = 2) out vec4 fs_colour;

layout(set = 0, binding = 0) uniform UBO1 {
    mat4 modelViewMatrix;
    mat4 projectionMatrix;
    vec2 resolution;
    vec4 frontfaceColour;
    vec4 backfaceColour;
    bool depthTestEnabled;
    bool useColour;
};

void main() {
    fs_normal = normal;
    fs_texture = texture;
    fs_colour = colour;

    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}