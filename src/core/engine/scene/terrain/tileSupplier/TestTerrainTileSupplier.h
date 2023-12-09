#ifndef WORLDENGINE_TESTTERRAINTILESUPPLIER_H
#define WORLDENGINE_TESTTERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/engine/scene/terrain/TerrainTileSupplier.h"

class ImageData;
class Image2D;

class TestTerrainTileSupplier : public TerrainTileSupplier {
    NO_COPY(TestTerrainTileSupplier);
    NO_MOVE(TestTerrainTileSupplier);
private:
    using TileIterator = std::unordered_map<glm::uvec4, TileData*>::iterator;

public:
    TestTerrainTileSupplier(ImageData* heightmapImageData);

    virtual ~TestTerrainTileSupplier() override;

    virtual void update() override;

    virtual const std::vector<ImageView*>& getLoadedTileImageViews() const override;

    virtual TileDataReference getTile(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) override;


private:
    TileIterator markIdle(const TileIterator& it);

    TileIterator markActive(const TileIterator& it);

    void requestTileData(TileData* tileData);

    glm::uvec2 getTileTextureSize(const glm::dvec2& normalizedSize) const;

    glm::uvec2 getLowerTexelCoord(const glm::dvec2& normalizedCoord) const;

    glm::uvec2 getUpperTexelCoords(const glm::dvec2& normalizedCoord) const;

    glm::uvec4 getTileId(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) const;

    void computeTerrainTileHeightRange(TileData* tileData);

    void computeTerrainTileMipLevel(glm::uvec2 minCoord, glm::uvec2 maxCoord, uint32_t level);

private:

    ImageData* m_heightmapImageData;
    std::shared_ptr<Image2D> m_heightmapImage;
    std::shared_ptr<ImageView> m_heightmapImageView;
    std::vector<ImageView*> m_loadedTileImageViews;

    std::unordered_map<glm::uvec4, TileData*> m_activeTiles;
    std::unordered_map<glm::uvec4, TileData*> m_idleTiles;

    std::vector<TileData*> m_requestedTilesQueue;
    std::vector<TileData*> m_pendingTilesQueue;

    float m_tileIdleTimeoutSeconds; // How many seconds until an unused active tile becomes idle.
    float m_tileExpireTimeoutSeconds; // How many seconds until an idle tile expires and is deleted.
};


#endif //WORLDENGINE_TESTTERRAINTILESUPPLIER_H
