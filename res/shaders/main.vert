#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texture;

layout(set = 0, binding = 0) uniform UBO1 {
    mat4 modelMatrix;
};

layout(set = 1, binding = 0) uniform UBO2 {
    mat4 viewProjectionMatrix;
};

void main() {
    gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 1.0);
}