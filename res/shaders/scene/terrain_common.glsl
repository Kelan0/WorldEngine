#include "shaders/common/structures.glsl"

#define UINT_MAX 0xFFFFFFFF

struct TerrainTileData {
    vec2 tilePosition;
    vec2 tileSize;
    vec2 textureOffset;
    vec2 textureSize;
};

ivec2 calculateTerrainTileGridPos(int vertexIndex, int tileGridSize) {
    int verticesPerStrip = 2 + tileGridSize * 2 + 2;
    int rowIndex = vertexIndex / verticesPerStrip;
    int stripIndex = vertexIndex % verticesPerStrip;

    ivec2 gridPos = ivec2(0);
    gridPos.x = stripIndex / 2;
    gridPos.y = rowIndex + stripIndex % 2;

    // Flatten the end triangles to create a degenerate triangle between strips
    if (stripIndex == 0)
    gridPos += 1;
    else if (stripIndex == verticesPerStrip - 1)
    gridPos -= 1;

    gridPos.x -= (1 - stripIndex % 2);

    return gridPos;
}

vec2 calculateTerrainTileNormalizedPos(ivec2 tilleGridPos, int tileGridSize) {
    return vec2(tilleGridPos) / float(tileGridSize);
}

vec4 calculateTerrainTileLocalPos(vec2 normalizedTilePos , vec2 tileOffset, vec2 tileSize) {
    vec4 localPos = vec4(0.0, 0.0, 0.0, 1.0);
    localPos.xz = tileOffset + normalizedTilePos * tileSize;
    return localPos;
}

vec2 calculateTerrainTileTexturePos(vec2 normalizedTilePos , vec2 textureOffset, vec2 textureSize) {
    vec2 texPos = textureOffset + normalizedTilePos * textureSize;
    return texPos;
}

float calculateTerrainTileElevation(sampler2D heightmapTex, vec2 texturePos, float elevationScale) {
    float elevation = texture(heightmapTex, texturePos.xy).r;
    elevation *= elevationScale;
    return elevation;
}