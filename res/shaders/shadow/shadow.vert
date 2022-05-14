#version 450

#extension GL_EXT_nonuniform_qualifier : enable

struct ObjectData {
    mat4 modelMatrix;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 texture;

layout(set = 0, binding = 0) uniform UBO1 {
    mat4 viewProjectionMatrix;
};

layout(set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
};

void main() {
    mat4 modelMatrix = objects[gl_InstanceIndex].modelMatrix;
    gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 1.0);
}