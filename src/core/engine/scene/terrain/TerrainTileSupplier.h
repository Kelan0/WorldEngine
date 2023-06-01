
#ifndef WORLDENGINE_TERRAINTILESUPPLIER_H
#define WORLDENGINE_TERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/util/IdManager.h"
#include "core/util/Time.h"

class Image2D;
class ImageView;

class TerrainTileSupplier;


struct TileData {
    uint32_t id;
    uint32_t referenceCount;
    uint32_t tileTextureIndex;
    glm::dvec2 tileOffset;
    glm::dvec2 tileSize;
    float minHeight;
    float maxHeight;
    float* heightData;
    Time::moment_t lastActive;
    union {
        uint8_t _flags;
        struct {
            bool requested : 1;
            bool pending : 1;
            bool available : 1;
        };
    };

    TileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize);

    ~TileData();
};

class TileDataReference {
public:
    explicit TileDataReference(TileData* tileData);
    TileDataReference(const TileDataReference& copy);
    TileDataReference(TileDataReference&& move) noexcept;

    ~TileDataReference();

    TileDataReference& operator=(const TileDataReference& copy);
    TileDataReference& operator=(TileDataReference&& move) noexcept;

    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(const TileDataReference& rhs) const;
    bool operator!=(const TileDataReference& rhs) const;

    const glm::dvec2& getTileOffset() const;

    const glm::dvec2& getTileSize() const;

    float getMinHeight() const;

    float getMaxHeight() const;

private:
    TileData* m_tileData;
};



class TerrainTileSupplier {
public:
    TerrainTileSupplier();

    virtual ~TerrainTileSupplier();

    virtual void update() const = 0;

    virtual const std::vector<ImageView*>& getLoadedTileImageViews() const = 0;

    virtual uint32_t requestTileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) = 0;

    virtual void releaseTileData(uint32_t id) = 0;

    virtual TileDataReference getTile(uint32_t id) const = 0;

private:

};







class HeightmapTerrainTileSupplier : public TerrainTileSupplier {
public:
    explicit HeightmapTerrainTileSupplier(const std::shared_ptr<Image2D>& heightmapImage);

    ~HeightmapTerrainTileSupplier() override;

    void update() const override;

    const std::vector<ImageView*>& getLoadedTileImageViews() const override;

    uint32_t requestTileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) override;

    void releaseTileData(uint32_t id) override;

    TileDataReference getTile(uint32_t id) const override;

    const std::shared_ptr<Image2D>& getHeightmapImage() const;

    const std::shared_ptr<ImageView>& getHeightmapImageView() const;

private:
    glm::uvec2 getLowerTexelCoord(const glm::uvec2& normalizedCoord) const;

    glm::uvec2 getUpperTexelCoords(const glm::uvec2& normalizedCoord) const;

private:
    std::shared_ptr<Image2D> m_heightmapImage;
    std::shared_ptr<ImageView> m_heightmapImageView;
    std::vector<ImageView*> m_loadedTileImageViews;

    std::unordered_map<glm::uvec4, uint32_t> m_tileBounds;
    std::unordered_map<uint32_t, TileData*> m_loadedTiles;

    IdManager<uint32_t> m_idManager;
};


#endif //WORLDENGINE_TERRAINTILESUPPLIER_H
