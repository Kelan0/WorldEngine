#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"
#include "shaders/scene/terrain_common.glsl"


//layout(location = 0) in vec3 position;
//layout(location = 1) in vec3 normal;
//layout(location = 2) in vec3 tangent;
//layout(location = 3) in vec2 textureCoord;

layout(location = 0) out vec3 fs_normal;
layout(location = 1) out vec3 fs_tangent;
layout(location = 2) out vec3 fs_bitangent;
layout(location = 3) out vec2 fs_textureCoord;
layout(location = 4) out vec4 fs_prevPosition;
layout(location = 5) out vec4 fs_currPosition;
layout(location = 6) out flat uint fs_heightmapTextureIndex;


layout(std140, set = 0, binding = 0) uniform UBO1 {
    CameraData prevCamera;
    CameraData camera;
    vec2 taaPreviousJitterOffset;
    vec2 taaCurrentJitterOffset;
};

layout(std140, set = 1, binding = 0) uniform UBO2 {
    mat4 terrainTransformMatrix;
    vec4 terrainScale;
    uvec4 heightmapTextureIndex_tileGridSize;
};

layout(std140, set = 1, binding = 1) readonly buffer TerrainTileDataBuffer {
    TerrainTileData terrainTileData[];
};

layout(set = 1, binding = 2) uniform sampler2D heightmapTextures[];

void main() {
//    mat4 prevModelMatrix = mat4(1.0);
    TerrainTileData tileData = terrainTileData[gl_InstanceIndex];

    uint heightmapTextureIndex = heightmapTextureIndex_tileGridSize[0];
    int tileGridSize = int(heightmapTextureIndex_tileGridSize[1]);
    int vertexIndex = int(gl_VertexIndex); // gl_VertexID for OpenGL

    ivec2 gridPos = calculateTerrainTileGridPos(vertexIndex, tileGridSize);
    vec2 normPos = calculateTerrainTileNormalizedPos(gridPos, tileGridSize);

    vec4 localPos = calculateTerrainTileLocalPos(normPos, tileData.tilePosition, tileData.tileSize);
    vec2 localTex = calculateTerrainTileTexturePos(normPos, tileData.textureOffset, tileData.textureSize);

    if (heightmapTextureIndex != UINT_MAX) {
        localPos.y = calculateTerrainTileElevation(heightmapTextures[heightmapTextureIndex], localTex, terrainScale.z);
    }

    mat4 modelMatrix = terrainTransformMatrix;// * terrainTileMatrix;
    mat3 normalMatrix = transpose(inverse(mat3(camera.viewMatrix) * mat3(modelMatrix)));
    vec4 worldPos = modelMatrix * localPos;
    vec3 worldNormal = normalize(normalMatrix * vec3(0, 1, 0));
    vec3 worldTangent = normalize(normalMatrix * vec3(1, 0, 0));
    vec3 worldBitangent = cross(worldNormal, worldTangent);


    fs_normal = worldNormal.xyz;
    fs_tangent = worldTangent.xyz;
    fs_bitangent = worldBitangent.xyz;
    fs_textureCoord = localTex;
    fs_currPosition = camera.viewProjectionMatrix * worldPos;
    fs_heightmapTextureIndex = heightmapTextureIndex;

    gl_Position = fs_currPosition;
}