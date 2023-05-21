#ifndef WORLDENGINE_QUADTREETERRAIN_H
#define WORLDENGINE_QUADTREETERRAIN_H

#include "core/core.h"

class TerrainTileQuadtree;

class QuadtreeTerrainComponent {
public:
    QuadtreeTerrainComponent();

    ~QuadtreeTerrainComponent();

    QuadtreeTerrainComponent& setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth);

    QuadtreeTerrainComponent& setTileResolution(uint32_t tileResolution);

    QuadtreeTerrainComponent& setSize(const glm::dvec2& size);

    QuadtreeTerrainComponent& setHeightScale(double heightScale);

    uint32_t getMaxQuadtreeDepth() const;

    uint32_t getTileResolution() const;

    const glm::dvec2& getSize() const;

    double getHeightScale() const;

    TerrainTileQuadtree* getTileQuadtree() const;

private:
    uint32_t m_maxQuadtreeDepth;
    uint32_t m_tileResolution;
    glm::dvec2 m_size;
    double m_heightScale;

    TerrainTileQuadtree* m_tileQuadtree;
};


class TerrainTileQuadtree {
public:

private:

};

#endif //WORLDENGINE_QUADTREETERRAIN_H
