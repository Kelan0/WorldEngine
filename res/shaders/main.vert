#version 450

#extension GL_EXT_nonuniform_qualifier : enable

struct ObjectData {
    mat4 modelMatrix;
    uint textureIndex;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texture;

layout(location = 0) out vec3 fs_normal;
layout(location = 1) out vec2 fs_texture;
layout(location = 2) out flat uint fs_textureIndex;

layout(set = 0, binding = 0) uniform UBO2 {
    mat4 viewProjectionMatrix;
};

layout(set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
};

void main() {
    mat4 modelMatrix = objects[gl_InstanceIndex].modelMatrix;

    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vec4 worldNormal = transpose(inverse(modelMatrix)) * vec4(normal, 0.0);
    fs_normal = worldNormal.xyz;
    fs_texture = texture;
    fs_textureIndex = objects[gl_InstanceIndex].textureIndex;
    
    gl_Position = viewProjectionMatrix * worldPos;
}