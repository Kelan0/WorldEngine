
#ifndef WORLDENGINE_TERRAINTILESUPPLIER_H
#define WORLDENGINE_TERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
#include "core/graphics/FrameResource.h"
#include "core/util/IdManager.h"
#include "core/util/Time.h"

class ImageData;
class Image2D;
class ImageView;
class Sampler;

class TerrainTileSupplier;
class TileDataReference;
class ComputePipeline;
class DescriptorSetLayout;
class DescriptorSet;
class Fence;

struct ShutdownGraphicsEvent;


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
    float cameraDistance;
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
    explicit HeightmapTerrainTileSupplier(const ImageData* heightmapImageData);

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

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

    bool init();

    bool updateTerrainTileHeightRangeDescriptors(const vk::CommandBuffer& commandBuffer, TileData* tileData);

    bool computeTerrainTileHeightRange(const vk::CommandBuffer& commandBuffer, TileData* tileData, Fence* fence);

    std::pair<vk::CommandBuffer, Fence*> getComputeCommandBuffer();

private:
    struct RequestTexture {
        Image2D* heightRangeTempImage;
        std::vector<ImageView*> heightRangeTempImageViews;
        DescriptorSet* descriptorSet;
        vk::CommandBuffer commandBuffer;
        Fence* fence;
        bool debugUsed;
        glm::uvec4 id;
    };

    struct TextureData {
        RequestTexture* requestTexture = nullptr;
    };

    RequestTexture* createRequestTexture(const vk::CommandBuffer& commandBuffer, uint32_t width, uint32_t height, uint32_t mipLevels);

    void deleteRequestTexture(RequestTexture* requestTexture);

    void makeRequestTextureAvailable(RequestTexture* requestTexture);

private:
    std::shared_ptr<Image2D> m_heightmapImage;
    std::shared_ptr<ImageView> m_heightmapImageView;
    std::vector<ImageView*> m_loadedTileImageViews;

    std::vector<uint32_t> m_availableTileTextureIndices;
    std::vector<TextureData> m_tileTextures;

    std::unordered_map<glm::uvec4, TileData*> m_activeTiles;
    std::unordered_map<glm::uvec4, TileData*> m_idleTiles;

    std::vector<TileData*> m_requestedTilesQueue;
    std::vector<TileData*> m_pendingTilesQueue;
    std::vector<RequestTexture*> m_availableRequestTextures;
    std::vector<std::pair<vk::CommandBuffer, Fence*>> m_availableCommandBuffers;

    bool m_initialized;
    DescriptorSet* m_tileComputeSharedDescriptorSet;
    uint32_t m_numUsedRequestTextureDescriptors;

    static SharedResource<DescriptorSetLayout> s_tileComputeSharedDescriptorSetLayout;
    static SharedResource<DescriptorSetLayout> s_tileComputeDescriptorSetLayout;
    static ComputePipeline* s_heightRangeComputePipeline;
    static Sampler* s_heightRangeComputeSampler;
};


#endif //WORLDENGINE_TERRAINTILESUPPLIER_H
