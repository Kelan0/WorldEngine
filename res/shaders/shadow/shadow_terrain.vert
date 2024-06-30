#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "shaders/common/structures.glsl"
#include "shaders/scene/terrain_common.glsl"

layout(set = 0, binding = 0) uniform UBO1 {
    CameraData camera;
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

    mat4 modelMatrix = terrainTransformMatrix;
    gl_Position = camera.viewProjectionMatrix * modelMatrix * localPos;
}