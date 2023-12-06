#ifndef WORLDENGINE_HEIGHTMAPTERRAINTILESUPPLIER_H
#define WORLDENGINE_HEIGHTMAPTERRAINTILESUPPLIER_H

#include "core/core.h"
#include "core/engine/scene/terrain/TerrainTileSupplier.h"


class ImageData;
class Image2D;
class ImageView;
class Sampler;
class ComputePipeline;
class DescriptorSetLayout;
class DescriptorSet;
class Fence;

class TerrainTileSupplier;
class TileDataReference;

struct ShutdownGraphicsEvent;


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

    glm::uvec2 getTileTextureSize(const glm::dvec2& normalizedSize) const;

    glm::uvec2 getLowerTexelCoord(const glm::dvec2& normalizedCoord) const;

    glm::uvec2 getUpperTexelCoords(const glm::dvec2& normalizedCoord) const;

    glm::uvec4 getTileId(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) const;

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

    bool init();

    bool computeTerrainTileHeightRange(const vk::CommandBuffer& commandBuffer, TileData* tileData, Fence* fence);

    std::pair<vk::CommandBuffer, Fence*> getComputeCommandBuffer();

private:
    struct RequestTexture {
        Image2D* heightRangeTempImage;
        std::vector<ImageView*> heightRangeTempImageViews;
        DescriptorSet* descriptorSet;
        vk::CommandBuffer commandBuffer;
        Fence* fence;
        bool writeDescriptors;
        bool debugUsed;
        glm::uvec4 id;
    };

    struct TextureData {
        RequestTexture* requestTexture = nullptr;
    };

    RequestTexture* createRequestTexture(uint32_t width, uint32_t height, uint32_t mipLevels);

    void writeRequestTextureDescriptors(const vk::CommandBuffer& commandBuffer, RequestTexture* requestTexture);

    bool assignRequestTexture(TileData* tileData);

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


#endif //WORLDENGINE_HEIGHTMAPTERRAINTILESUPPLIER_H
