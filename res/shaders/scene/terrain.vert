#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"

struct TerrainTileData {
    vec2 tilePosition;
    vec2 tileSize;
    vec2 textureOffset;
    vec2 textureSize;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 textureCoord;

layout(location = 0) out vec3 fs_normal;
layout(location = 1) out vec3 fs_tangent;
layout(location = 2) out vec3 fs_bitangent;
layout(location = 3) out vec2 fs_textureCoord;
layout(location = 4) out vec4 fs_prevPosition;
layout(location = 5) out vec4 fs_currPosition;
layout(location = 6) out flat uint fs_heightmapTextureIndex;

#define UINT_MAX 0xFFFFFFFF

layout(std140, set = 0, binding = 0) uniform UBO1 {
    CameraData prevCamera;
    CameraData camera;
    vec2 taaPreviousJitterOffset;
    vec2 taaCurrentJitterOffset;
};

layout(std140, set = 1, binding = 0) uniform UBO2 {
    mat4 terrainTransformMatrix;
    vec4 terrainScale;
    uvec4 heightmapTextureIndex;
};

layout(std140, set = 1, binding = 1) readonly buffer TerrainTileDataBuffer {
    TerrainTileData terrainTileData[];
};

layout(set = 1, binding = 2) uniform sampler2D heightmapTextures[];

void main() {
//    mat4 prevModelMatrix = mat4(1.0);
    TerrainTileData tileData = terrainTileData[gl_InstanceIndex];

    mat4 modelMatrix = terrainTransformMatrix;// * terrainTileMatrix;

    mat3 normalMatrix = transpose(inverse(mat3(camera.viewMatrix) * mat3(modelMatrix)));

    vec4 localPos = vec4(0.0, 0.0, 0.0, 1.0);
    localPos.xz = tileData.tilePosition + position.xz * tileData.tileSize;

    float vertexDistance = 1.0 / 17.0;
    vec2 localTex = tileData.textureOffset + textureCoord.xy * tileData.textureSize;
    vec3 localNorm = normal;

    if (heightmapTextureIndex[0] != UINT_MAX) {
        float elevationScale = terrainScale.z;//1.0;
        float elevation = texture(heightmapTextures[heightmapTextureIndex[0]], localTex.xy).r * elevationScale;
        localPos.y += elevation;
    }

    vec4 worldPos = modelMatrix * localPos;
    vec3 worldNormal = normalize(normalMatrix * localNorm);
    vec3 worldTangent = normalize(normalMatrix * tangent);
    vec3 worldBitangent = cross(worldNormal, worldTangent);

    fs_normal = worldNormal.xyz;
    fs_tangent = worldTangent.xyz;
    fs_bitangent = worldBitangent.xyz;
    fs_textureCoord = localTex;
//    fs_prevPosition = prevCamera.viewProjectionMatrix * prevModelMatrix * vec4(position, 1.0);
    fs_currPosition = camera.viewProjectionMatrix * worldPos;
    fs_heightmapTextureIndex = heightmapTextureIndex[0];

    gl_Position = fs_currPosition;
}