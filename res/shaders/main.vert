#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texture;

layout(set = 0, binding = 0) uniform TestUBO {
    mat4 modelMatrix;
} matrices;

void main() {
    gl_Position = matrices.modelMatrix * vec4(position, 1.0);
}