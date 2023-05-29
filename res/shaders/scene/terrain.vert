#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"

struct TerrainData {
    mat4 modelMatrix;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 texture;

layout(location = 0) out vec3 fs_normal;
layout(location = 1) out vec3 fs_tangent;
layout(location = 2) out vec3 fs_bitangent;
layout(location = 3) out vec2 fs_texture;
layout(location = 4) out vec4 fs_prevPosition;
layout(location = 5) out vec4 fs_currPosition;

layout(std140, set = 0, binding = 0) uniform UBO1 {
    CameraData prevCamera;
    CameraData camera;
    vec2 taaPreviousJitterOffset;
    vec2 taaCurrentJitterOffset;
};

layout(std140, set = 1, binding = 0) uniform UBO2 {
    mat4 terrainTransformMatrix;
//    vec4 terrainScale;
};

layout(std140, set = 1, binding = 1) readonly buffer TerrainDataBuffer {
    TerrainData terrainData[];
};

void main() {
//    mat4 prevModelMatrix = mat4(1.0);
    mat4 terrainTileMatrix = terrainData[gl_InstanceIndex].modelMatrix;
    mat4 modelMatrix = terrainTransformMatrix * terrainTileMatrix;

    mat3 normalMatrix = transpose(inverse(mat3(camera.viewMatrix) * mat3(modelMatrix)));

    vec4 worldPos = modelMatrix * vec4(position, 1.0);
    vec3 worldNormal = normalize(normalMatrix * normal);
    vec3 worldTangent = normalize(normalMatrix * tangent);
    vec3 worldBitangent = cross(worldNormal, worldTangent);

    fs_normal = worldNormal.xyz;
    fs_tangent = worldTangent.xyz;
    fs_bitangent = worldBitangent.xyz;
    fs_texture = texture;
//    fs_prevPosition = prevCamera.viewProjectionMatrix * prevModelMatrix * vec4(position, 1.0);
    fs_currPosition = camera.viewProjectionMatrix * worldPos;

    gl_Position = fs_currPosition;
}