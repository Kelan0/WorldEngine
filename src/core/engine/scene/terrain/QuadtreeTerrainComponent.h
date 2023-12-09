#ifndef WORLDENGINE_QUADTREETERRAINCOMPONENT_H
#define WORLDENGINE_QUADTREETERRAINCOMPONENT_H

#include "core/core.h"

class TerrainTileQuadtree;
class TerrainTileSupplier;

class QuadtreeTerrainComponent {
public:
    QuadtreeTerrainComponent();

    ~QuadtreeTerrainComponent();

    QuadtreeTerrainComponent& setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth);

    QuadtreeTerrainComponent& setTileResolution(uint32_t tileResolution);

    QuadtreeTerrainComponent& setTileGridSize(uint32_t tileGridSize);

    QuadtreeTerrainComponent& setSize(const glm::dvec2& size);

    QuadtreeTerrainComponent& setHeightScale(double heightScale);

    QuadtreeTerrainComponent& setTileSupplier(const std::shared_ptr<TerrainTileSupplier>& tileSupplier);

    uint32_t getMaxQuadtreeDepth() const;

    uint32_t getTileResolution() const;

    uint32_t getTileGridSize() const;

    const glm::dvec2& getSize() const;

    double getHeightScale() const;

    const std::shared_ptr<TerrainTileSupplier>& getTileSupplier() const;

    const std::unique_ptr<TerrainTileQuadtree>& getTileQuadtree() const;

private:
    uint32_t m_tileResolution;
    uint32_t m_tileGridSize;
    std::unique_ptr<TerrainTileQuadtree> m_tileQuadtree;
};


#endif //WORLDENGINE_QUADTREETERRAINCOMPONENT_H
