
#ifndef WORLDENGINE_TERRAINTILESUPPLIER_H
#define WORLDENGINE_TERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/util/IdManager.h"
#include "core/util/Time.h"

class Image2D;
class ImageView;

class TerrainTileSupplier;
class TileDataReference;


struct TileData {
    uint32_t referenceCount;
    uint32_t tileTextureIndex;
    glm::dvec2 tileOffset;
    glm::dvec2 tileSize;
    float minHeight;
    float maxHeight;
    float* heightData;
    glm::uvec2 heightDataResolution;
    Time::moment_t timeLastUsed;
    float cameraDistance;
    union {
        uint8_t _flags;
        struct {
            bool requested : 1;
            bool pending : 1;
            bool available : 1;
            bool idle: 1;
            bool deleted: 1;
        };
    };

    TileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize);

    ~TileData();
};

class TileDataReference {
public:
    explicit TileDataReference(TileData* tileData);
    explicit TileDataReference(std::nullptr_t);
    TileDataReference(const TileDataReference& copy);
    TileDataReference(TileDataReference&& move) noexcept;

    ~TileDataReference();

    TileDataReference& operator=(std::nullptr_t);
    TileDataReference& operator=(const TileDataReference& copy);
    TileDataReference& operator=(TileDataReference&& move) noexcept;

    bool operator==(std::nullptr_t) const;
    bool operator!=(std::nullptr_t) const;
    bool operator==(const TileDataReference& rhs) const;
    bool operator!=(const TileDataReference& rhs) const;

    void notifyUsed();

    bool valid() const;

    bool release();

    void setCameraDistance(float cameraDistance);

    bool isAvailable() const;

    const glm::dvec2& getTileOffset() const;

    const glm::dvec2& getTileSize() const;

    float getMinHeight() const;

    float getMaxHeight() const;

    static void invalidateAllReferences(TileData* tileData);
private:
    struct ReferenceTracker {
        uint32_t strongReferences;
        uint32_t weakReferences;
    };
private:
//    ReferenceTracker* m_tracker;
    TileData* m_tileData;
};



class TerrainTileSupplier {
    NO_COPY(TerrainTileSupplier);
    NO_MOVE(TerrainTileSupplier);
public:
    TerrainTileSupplier();

    virtual ~TerrainTileSupplier();

    virtual void update() = 0;

    virtual const std::vector<ImageView*>& getLoadedTileImageViews() const = 0;

    virtual TileDataReference getTile(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) = 0;

private:

};







class HeightmapTerrainTileSupplier : public TerrainTileSupplier {
    NO_COPY(HeightmapTerrainTileSupplier);
    NO_MOVE(HeightmapTerrainTileSupplier);
private:
    using TileIterator = std::unordered_map<glm::uvec4, TileData*>::iterator;
public:
    explicit HeightmapTerrainTileSupplier(const std::shared_ptr<Image2D>& heightmapImage);

    ~HeightmapTerrainTileSupplier() override;

    void update() override;

    const std::vector<ImageView*>& getLoadedTileImageViews() const override;

    TileDataReference getTile(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) override;

    const std::shared_ptr<Image2D>& getHeightmapImage() const;

    const std::shared_ptr<ImageView>& getHeightmapImageView() const;

private:
    TileIterator markIdle(const TileIterator& it);

    TileIterator markActive(const TileIterator& it);

    void requestTileData(TileData* tileData);

    glm::uvec2 getLowerTexelCoord(const glm::dvec2& normalizedCoord) const;

    glm::uvec2 getUpperTexelCoords(const glm::dvec2& normalizedCoord) const;

    glm::uvec4 getTileId(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) const;

private:
    std::shared_ptr<Image2D> m_heightmapImage;
    std::shared_ptr<ImageView> m_heightmapImageView;
    std::vector<ImageView*> m_loadedTileImageViews;

    std::unordered_map<glm::uvec4, TileData*> m_activeTiles;
    std::unordered_map<glm::uvec4, TileData*> m_idleTiles;
};


#endif //WORLDENGINE_TERRAINTILESUPPLIER_H
