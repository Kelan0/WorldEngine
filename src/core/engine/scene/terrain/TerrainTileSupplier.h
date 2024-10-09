
#ifndef WORLDENGINE_TERRAINTILESUPPLIER_H
#define WORLDENGINE_TERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
#include "core/graphics/FrameResource.h"
#include "core/util/IdManager.h"
#include "core/util/Time.h"

class ImageView;

class TerrainTileSupplier;
class TileDataReference;



struct TileData {
    enum State : uint8_t {
        State_None = 0,
        State_Requested = 1,
        State_Pending = 2,
        State_Available = 3,
    };

    TerrainTileSupplier* tileSupplier;
    uint32_t tileTextureIndex;
    uint32_t referenceCount;
    glm::dvec2 tileOffset;
    glm::dvec2 tileSize;
    float minHeight;
    float maxHeight;
    float* heightData;
    glm::uvec2 heightDataResolution;
    Time::moment_t timeLastUsed;
    Time::moment_t timeRequested;
    Time::moment_t timeProcessed;
    float priority;
    union {
        uint8_t _flags;
        struct {
            State state : 2;
            bool idle: 1;
            bool deleted: 1;
        };
    };

    TileData(TerrainTileSupplier* tileSupplier, uint32_t tileTextureIndex, const glm::dvec2& tileOffset, const glm::dvec2& tileSize);

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

    // Priority for loading this tile. Higher priority tiles are loaded before lower priority tiles.
    void setPriority(float priority);

    float getPriority() const;

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








#endif //WORLDENGINE_TERRAINTILESUPPLIER_H
