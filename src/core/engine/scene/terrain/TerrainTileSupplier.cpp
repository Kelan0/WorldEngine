
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Texture.h"
#include "core/application/Engine.h"
#include "core/util/Logger.h"
#include "core/graphics/Fence.h"

//size_t allocCounter = 0;

glm::uvec4 debugGetTileId(const glm::dvec2& offset, const glm::dvec2& size) {
    return glm::uvec4((uint32_t)glm::floor(8192.0 * offset.x),
                      (uint32_t)glm::floor(8192.0 * offset.y),
                      (uint32_t)glm::ceil(8192.0 * (offset.x + size.x)),
                      (uint32_t)glm::ceil(8192.0 * (offset.y + size.y)));
}

TileData::TileData(TerrainTileSupplier* tileSupplier, uint32_t tileTextureIndex, const glm::dvec2& tileOffset, const glm::dvec2& tileSize) :
        tileSupplier(tileSupplier),
        tileTextureIndex(tileTextureIndex),
        referenceCount(0),
        tileOffset(tileOffset),
        tileSize(tileSize),
        minHeight(0.0),
        maxHeight(1.0),
        heightDataResolution(0, 0),
        heightData(nullptr),
        timeLastUsed(Time::now()),
        cameraDistance(INFINITY),
        state(State_None),
        idle(false),
        deleted(false) {

    assert(tileOffset.x >= 0.0 && tileOffset.y >= 0.0);
    assert(tileOffset.x + tileSize.x <= 1.0 && tileOffset.y + tileSize.y <= 1.0);

//    glm::uvec4 id = debugGetTileId(tileOffset, tileSize);
//    ++allocCounter;
//    LOG_DEBUG("TileData [%u %u, %u %u] constructed - %zu tiles exist", id.x, id.y, id.y, id.z, allocCounter);
}

TileData::~TileData() {
//    glm::uvec4 id = debugGetTileId(tileOffset, tileSize);
//    --allocCounter;
//    LOG_DEBUG("TileData [%u %u, %u %u] deleted - %zu tiles exist", id.x, id.y, id.y, id.z, allocCounter);
}


TileDataReference::TileDataReference(TileData* tileData) :
        m_tileData(tileData) {
    if (m_tileData != nullptr)
        ++m_tileData->referenceCount;
}

TileDataReference::TileDataReference(std::nullptr_t) :
        m_tileData(nullptr) {

}

TileDataReference::TileDataReference(const TileDataReference& copy) :
        m_tileData(copy.m_tileData) {
    if (m_tileData != nullptr)
        ++m_tileData->referenceCount;
}

TileDataReference::TileDataReference(TileDataReference&& move) noexcept:
        m_tileData(std::exchange(move.m_tileData, nullptr)) {
}

TileDataReference::~TileDataReference() {
    release();
}

TileDataReference& TileDataReference::operator=(std::nullptr_t) {
    release();
    return *this;
}

TileDataReference& TileDataReference::operator=(const TileDataReference& copy) {
    if (&copy != this) {
        m_tileData = copy.m_tileData;
        if (m_tileData != nullptr)
            ++m_tileData->referenceCount;
    }
    return *this;
}

TileDataReference& TileDataReference::operator=(TileDataReference&& move) noexcept {
    if (&move != this) {
        m_tileData = std::exchange(move.m_tileData, nullptr);
    }
    return *this;
}

bool TileDataReference::operator==(std::nullptr_t) const {
//    return m_tileData == nullptr;
    return !valid();
}

bool TileDataReference::operator!=(std::nullptr_t) const {
//    return m_tileData != nullptr;
    return valid();
}

bool TileDataReference::operator==(const TileDataReference& rhs) const {
    return m_tileData == rhs.m_tileData;
}

bool TileDataReference::operator!=(const TileDataReference& rhs) const {
    return m_tileData != rhs.m_tileData;
}

void TileDataReference::notifyUsed() {
    assert(valid());
    m_tileData->timeLastUsed = Time::now();
}

bool TileDataReference::valid() const {
    return m_tileData != nullptr && !m_tileData->deleted;
}

bool TileDataReference::release() {
    if (m_tileData != nullptr) {
        assert(m_tileData->referenceCount > 0);
        --m_tileData->referenceCount;
        if (m_tileData->referenceCount == 0)
            invalidateAllReferences(m_tileData);
        m_tileData = nullptr;
        return true;
    }
    return false;
}

void TileDataReference::setCameraDistance(float cameraDistance) {
    assert(valid());
    m_tileData->cameraDistance = cameraDistance;
}

bool TileDataReference::isAvailable() const {
    return valid() && m_tileData->state == TileData::State_Available;
}

const glm::dvec2& TileDataReference::getTileOffset() const {
    assert(valid());
    return m_tileData->tileOffset;
}

const glm::dvec2& TileDataReference::getTileSize() const {
    assert(valid());
    return m_tileData->tileSize;
}

float TileDataReference::getMinHeight() const {
    assert(valid());
    return m_tileData->minHeight;
}

float TileDataReference::getMaxHeight() const {
    assert(valid());
    return m_tileData->maxHeight;
}

void TileDataReference::invalidateAllReferences(TileData* tileData) {
    glm::uvec4 id = debugGetTileId(tileData->tileOffset, tileData->tileSize);
    if (!tileData->deleted) {
        tileData->deleted = true;
//        LOG_DEBUG("Marked tile [%u %u, %u %u] as deleted with %u references", id.x, id.y, id.z, id.w, tileData->referenceCount);
    }
    if (tileData->referenceCount == 0) {
//        LOG_DEBUG("Tile [%u %u, %u %u] reference count is zero. Deleting", id.x, id.y, id.z, id.w);
        delete tileData;
    }
}







TerrainTileSupplier::TerrainTileSupplier() {

};

TerrainTileSupplier::~TerrainTileSupplier() {
}









struct TerrainTileHeightRangePushConstants {
    glm::ivec2 dstResolution;
    glm::ivec2 tileOffset;
    uint32_t level;
    uint32_t srcImageIndex;
    uint32_t dstImageIndex;
};



SharedResource<DescriptorSetLayout> HeightmapTerrainTileSupplier::s_tileComputeSharedDescriptorSetLayout = nullptr;
SharedResource<DescriptorSetLayout> HeightmapTerrainTileSupplier::s_tileComputeDescriptorSetLayout = nullptr;
ComputePipeline* HeightmapTerrainTileSupplier::s_heightRangeComputePipeline = nullptr;
Sampler* HeightmapTerrainTileSupplier::s_heightRangeComputeSampler = nullptr;



HeightmapTerrainTileSupplier::HeightmapTerrainTileSupplier(const ImageData* heightmapImageData) :
        TerrainTileSupplier(),
        m_heightmapImageView(nullptr),
        m_tileComputeSharedDescriptorSet(nullptr),
        m_numUsedRequestTextureDescriptors(0),
        m_initialized(false) {

    Image2DConfiguration heightmapImageConfig{};
    heightmapImageConfig.device = Engine::graphics()->getDevice();
    heightmapImageConfig.filePath = "terrain/heightmap_1/heightmap2.hdr";
    heightmapImageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    heightmapImageConfig.format = vk::Format::eR32Sfloat;
    m_heightmapImage = std::shared_ptr<Image2D>(Image2D::create(heightmapImageConfig, "HeightmapTerrainTileSupplier-TerrainHeightmapImage"));

    ImageViewConfiguration heightmapImageViewConfig{};
    heightmapImageViewConfig.device = Engine::graphics()->getDevice();
    heightmapImageViewConfig.format = heightmapImageConfig.format;
    heightmapImageViewConfig.setImage(m_heightmapImage.get());
    m_heightmapImageView = std::shared_ptr<ImageView>(ImageView::create(heightmapImageViewConfig, "HeightmapTerrainTileSupplier-TerrainHeightmapImageView"));

    m_loadedTileImageViews.emplace_back(m_heightmapImageView.get());
}

HeightmapTerrainTileSupplier::~HeightmapTerrainTileSupplier() {
    delete m_tileComputeSharedDescriptorSet;

    for (RequestTexture* requestTexture : m_availableRequestTextures)
        deleteRequestTexture(requestTexture);

    std::set<Fence*> fences;

    for (TextureData& textureData : m_tileTextures) {
        if (textureData.requestTexture != nullptr) {
            if (textureData.requestTexture->fence != nullptr) {
                fences.insert(textureData.requestTexture->fence);
                textureData.requestTexture->fence = nullptr;
            }

            deleteRequestTexture(textureData.requestTexture);
        }
    }
    for (Fence* fence : fences)
        delete fence;

    for (auto& [commandBuffer, fence] : m_availableCommandBuffers)
        delete fence;
}

void HeightmapTerrainTileSupplier::update() {

    if (!m_initialized)
        init();

    auto now = Time::now();
    uint64_t tileIdleTimeoutNanoseconds = (uint64_t)(10 * 1e+9); // 30 seconds
    uint64_t tileExpireTimeoutNanoseconds = (uint64_t)(30 * 1e+9); // 3 minutes


//    m_requestedTilesQueue.erase(std::remove_if(m_requestedTilesQueue.begin(), m_requestedTilesQueue.end(), [](const TileData* tile) {
//        return tile->state != TileData::State_Requested || tile->idle;
//    }), m_requestedTilesQueue.end());

    m_requestedTilesQueue.clear();


    // Deallocate tiles that have been idle for too long.
    for (auto it = m_idleTiles.begin(); it != m_idleTiles.end();) {
        glm::uvec4 id = it->first;
        TileData* tileData = it->second;

        if (!tileData->deleted && Time::nanoseconds(tileData->timeLastUsed, now) > tileExpireTimeoutNanoseconds) {
            tileData->deleted = true;
        }

        if (!tileData->deleted) {
            ++it;
            continue;
        }

        if (tileData->tileTextureIndex != UINT32_MAX) {
            assert(tileData->tileTextureIndex < m_tileTextures.size());
            m_availableTileTextureIndices.emplace_back(tileData->tileTextureIndex);
        }
        TileDataReference::invalidateAllReferences(tileData);
        it = m_idleTiles.erase(it);
    }

    // TODO: keep availableTileTextureIndices array sorted, and always reuse the lowest index.
    // TODO: shrink availableTileTextureIndices array if possible, actually deallocate the images at the end

    // Update the active tiles
    for (auto it = m_activeTiles.begin(); it != m_activeTiles.end();) {
        glm::uvec4 id = it->first;
        TileData* tileData = it->second;

        assert(tileData->referenceCount > 0 && !tileData->deleted && !tileData->idle);

        if (Time::nanoseconds(tileData->timeLastUsed, now) > tileIdleTimeoutNanoseconds) {
            it = markIdle(it);
            continue;
        }

        if (tileData->state == TileData::State_Requested) {
            if (tileData->tileTextureIndex == UINT32_MAX) {
                if (!m_availableTileTextureIndices.empty()) {
                    tileData->tileTextureIndex = m_availableTileTextureIndices.back();
                    m_availableTileTextureIndices.pop_back();

                } else {
                    tileData->tileTextureIndex = (uint32_t)m_tileTextures.size();
                    m_tileTextures.emplace_back(TextureData{});
                }
            }

            tileData->state = TileData::State_Pending;
            tileData->timeRequested = Time::now();
            m_requestedTilesQueue.emplace_back(tileData);
        }

        ++it;
    }

    if (!m_requestedTilesQueue.empty()) {

        const vk::Queue& queue = **Engine::graphics()->getQueue(QUEUE_COMPUTE_MAIN);
        vk::SubmitInfo queueSubmitInfo{};
        queueSubmitInfo.setCommandBufferCount(1);

        vk::CommandBufferBeginInfo commandBeginInfo{};
        commandBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        auto [commandBuffer, fence] = getComputeCommandBuffer();
        commandBuffer.begin(commandBeginInfo);

        // From the end of the request queue, assign the needed descriptors for each tile until we cannot do so.
        int32_t i = (int32_t)(m_requestedTilesQueue.size() - 1);
        for (; i >= 0; --i) {
            TileData* tile = m_requestedTilesQueue[i];
            if (!updateTerrainTileHeightRangeDescriptors(commandBuffer, tile))
                break;
            m_tileTextures[m_requestedTilesQueue[i]->tileTextureIndex].requestTexture->id = getTileId(tile->tileOffset, tile->tileSize);
        }

        ++i; // The last iteration overstepped

        auto it = std::next(m_requestedTilesQueue.begin(), i);
        LOG_DEBUG("Computing %zu of %zu requested tiles", (size_t)std::distance(it, m_requestedTilesQueue.end()), m_requestedTilesQueue.size());

        // For each tile that has a descriptor assigned, dispatch the compute commands
        for (auto it1 = it; it1 != m_requestedTilesQueue.end(); ++it1) {
            TileData* tile = *it1;
            tile->timeProcessed = Time::now();

            if (!commandBuffer) {
                std::tie(commandBuffer, fence) = getComputeCommandBuffer();
                commandBuffer.begin(commandBeginInfo);
            }
            computeTerrainTileHeightRange(commandBuffer, *it1, fence);
            commandBuffer.end();

            queueSubmitInfo.setPCommandBuffers(&commandBuffer);
            vk::Result result = queue.submit(1, &queueSubmitInfo, fence->getFence());
            assert(result == vk::Result::eSuccess);
            commandBuffer = nullptr;
        }


        // Pop this many items from the end of the request queue.
        m_requestedTilesQueue.erase(it, m_requestedTilesQueue.end());

    }

    std::set<Fence*> signaledFences;

    for (auto it = m_pendingTilesQueue.rbegin(); it != m_pendingTilesQueue.rend(); it++) {
        TileData* tile = *it;
        TextureData& textureData = m_tileTextures[tile->tileTextureIndex];
        RequestTexture* requestTexture = textureData.requestTexture;
        assert(requestTexture != nullptr);
        assert(requestTexture->debugUsed);
        assert(requestTexture->id == getTileId(tile->tileOffset, tile->tileSize));

        if (requestTexture->fence->getStatus() == Fence::Status_NotSignaled) {
            continue;
        }

        requestTexture->debugUsed = false;

        if (signaledFences.insert(requestTexture->fence).second) {
            assert(requestTexture->commandBuffer);
            m_availableCommandBuffers.emplace_back(requestTexture->commandBuffer, requestTexture->fence);
        }

        float pixels[2];

        ImageRegion region{};
        region.setOffset(0, 0, 0);
        region.setSize(1, 1, 1);
        region.baseMipLevel = requestTexture->heightRangeTempImage->getMipLevelCount() - 1;
        region.mipLevelCount = 1;
        ImageTransitionState srcState(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite, vk::PipelineStageFlagBits::eComputeShader);
        ImageTransitionState dstState(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite, vk::PipelineStageFlagBits::eComputeShader);
        requestTexture->heightRangeTempImage->readPixels(pixels, ImagePixelLayout::RG, ImagePixelFormat::Float32, vk::ImageAspectFlagBits::eColor, region, srcState, dstState);
        tile->minHeight = pixels[0];
        tile->maxHeight = pixels[1];

        glm::uvec4 id = getTileId(tile->tileOffset, tile->tileSize);
        LOG_DEBUG("Received tile data [%u %u, %u %u] - Processed after for %.2f msec, Processing took %.2f msec, Tile available after %.2f msec", id.x, id.y, id.z, id.w, Time::milliseconds(tile->timeRequested, tile->timeProcessed), Time::milliseconds(tile->timeProcessed, Time::now()), Time::milliseconds(tile->timeRequested, Time::now()));

        makeRequestTextureAvailable(requestTexture);
        tile->state = TileData::State_Available;
        textureData.requestTexture = nullptr;
        requestTexture->fence = nullptr;
        requestTexture->commandBuffer = nullptr;
    }

    m_pendingTilesQueue.erase(std::remove_if(m_pendingTilesQueue.begin(), m_pendingTilesQueue.end(), [](const TileData* tile) {
        return tile->state != TileData::State_Pending;
    }), m_pendingTilesQueue.end());

    Fence::resetFences(**Engine::graphics()->getDevice(), signaledFences.begin(), signaledFences.end());
}

const std::vector<ImageView*>& HeightmapTerrainTileSupplier::getLoadedTileImageViews() const {
    return m_loadedTileImageViews;
}

TileDataReference HeightmapTerrainTileSupplier::getTile(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) {
    glm::uvec4 id = getTileId(tileOffset, tileSize);

    TileData* tileData;

    auto it = m_activeTiles.find(id);
    if (it != m_activeTiles.end()) {
        // Tile found in active list. Use it.
        tileData = it->second;
        return TileDataReference(tileData);
    }

    auto it1 = m_idleTiles.find(id);
    if (it1 != m_idleTiles.end()) {
        // Tile was found in the idle list. Move it back to the active list.
        tileData = it1->second;

        assert(tileData != nullptr);
        assert(getTileId(tileData->tileOffset, tileData->tileSize) == id);

        markActive(it1);

        return TileDataReference(tileData);
    }

    // Tile was not already loaded. Request it.
    auto result = m_activeTiles.insert(std::make_pair(id, new TileData(this, UINT32_MAX, tileOffset, tileSize)));
    assert(result.second && "Tile ID conflict");

    tileData = result.first->second;
    tileData->idle = false;
    tileData->referenceCount = 1;

    requestTileData(tileData);
    return TileDataReference(tileData);
}


const std::shared_ptr<Image2D>& HeightmapTerrainTileSupplier::getHeightmapImage() const {
    return m_heightmapImage;
}

const std::shared_ptr<ImageView>& HeightmapTerrainTileSupplier::getHeightmapImageView() const {
    return m_heightmapImageView;
}

HeightmapTerrainTileSupplier::TileIterator HeightmapTerrainTileSupplier::markIdle(const TileIterator& it) {
    glm::uvec4 id = it->first;
    TileData* tile = it->second;

    assert(tile != nullptr);
    assert(!tile->idle);

    assert(tile->referenceCount > 0);
    --tile->referenceCount; // Remove the fake reference so this tile can be deleted if needed.

    if (tile->state == TileData::State_Requested)
        tile->state = TileData::State_None; // State did not progress to pending or available, so un-request this tile.
    tile->idle = true;

//    LOG_DEBUG("Tile [%u %u, %u %u] became idle after %.2f seconds", id.x, id.y, id.z, id.w, Time::milliseconds(tile->timeLastUsed, Time::now()) * 0.001);

    auto it1 = m_activeTiles.erase(it);
    m_idleTiles.insert(std::make_pair(id, tile));
    return it1;
}

HeightmapTerrainTileSupplier::TileIterator HeightmapTerrainTileSupplier::markActive(const TileIterator& it) {
    glm::uvec4 id = it->first;
    TileData* tile = it->second;

    assert(tile != nullptr);
    assert(tile->idle);

    if (tile->state == TileData::State_None)
        requestTileData(tile);

    tile->idle = false;
    ++tile->referenceCount; // Hold a fake reference to keep this tile alive while it's active.

//    LOG_DEBUG("Tile [%u %u, %u %u] became active again", id.x, id.y, id.z, id.w);
    tile->timeLastUsed = Time::now();

    auto it1 = m_idleTiles.erase(it);
    m_activeTiles.insert(std::make_pair(id, tile));
    return it1;
}

void HeightmapTerrainTileSupplier::requestTileData(TileData* tileData) {
    if (tileData->state == TileData::State_None)
        tileData->state = TileData::State_Requested;
}

glm::uvec2 HeightmapTerrainTileSupplier::getLowerTexelCoord(const glm::dvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(0);
    return glm::uvec2((uint32_t)glm::floor((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::floor((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}

glm::uvec2 HeightmapTerrainTileSupplier::getUpperTexelCoords(const glm::dvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(1);
    return glm::uvec2((uint32_t)glm::ceil((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::ceil((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}

glm::uvec4 HeightmapTerrainTileSupplier::getTileId(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) const {
    glm::uvec2 texelBoundMin = getLowerTexelCoord(tileOffset);
    glm::uvec2 texelBoundMax = getUpperTexelCoords(tileOffset + tileSize);
    return glm::uvec4(texelBoundMin, texelBoundMax);
}

void HeightmapTerrainTileSupplier::onCleanupGraphics(ShutdownGraphicsEvent* event) {
    s_tileComputeSharedDescriptorSetLayout.reset();
    s_tileComputeDescriptorSetLayout.reset();
    delete s_heightRangeComputePipeline;
    delete s_heightRangeComputeSampler;
}

bool HeightmapTerrainTileSupplier::init() {
    if (m_initialized)
        return true;

    m_initialized = true;

    if (s_tileComputeSharedDescriptorSetLayout == nullptr) {
        s_tileComputeSharedDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eCompute, 1)
                .build("HeightmapTerrainTileSupplier-TileComputeSharedDescriptorSetLayout");

        Engine::eventDispatcher()->connect(&HeightmapTerrainTileSupplier::onCleanupGraphics);
    }

    if (s_tileComputeDescriptorSetLayout == nullptr) {
        s_tileComputeDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                .addStorageImage(0, vk::ShaderStageFlagBits::eCompute, 16)
                .addStorageImage(1, vk::ShaderStageFlagBits::eCompute, 16)
                .build("HeightmapTerrainTileSupplier-TileComputeDescriptorSetLayout");

        Engine::eventDispatcher()->connect(&HeightmapTerrainTileSupplier::onCleanupGraphics);
    }

    if (s_heightRangeComputePipeline == nullptr) {
        ComputePipelineConfiguration heightRangeComputePipelineConfig{};
        heightRangeComputePipelineConfig.device = Engine::graphics()->getDevice();
        heightRangeComputePipelineConfig.computeShader = "shaders/terrain/compute_terrainTileHeightRange.glsl";
        heightRangeComputePipelineConfig.addDescriptorSetLayout(s_tileComputeSharedDescriptorSetLayout.get());
        heightRangeComputePipelineConfig.addDescriptorSetLayout(s_tileComputeDescriptorSetLayout.get());
        heightRangeComputePipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(TerrainTileHeightRangePushConstants));
        s_heightRangeComputePipeline = ComputePipeline::create(heightRangeComputePipelineConfig, "HeightmapTerrainTileSupplier-HeightRangeComputePipeline");

        Engine::eventDispatcher()->connect(&HeightmapTerrainTileSupplier::onCleanupGraphics);
    }

    if (s_heightRangeComputeSampler == nullptr) {
        SamplerConfiguration heightRangeComputeSamplerConfig{};
        heightRangeComputeSamplerConfig.device = Engine::graphics()->getDevice();
        heightRangeComputeSamplerConfig.minFilter = vk::Filter::eNearest;
        heightRangeComputeSamplerConfig.magFilter = vk::Filter::eNearest;
        s_heightRangeComputeSampler = Sampler::create(heightRangeComputeSamplerConfig, "HeightmapTerrainTileSupplier-HeightRangeComputePipeline");

        Engine::eventDispatcher()->connect(&HeightmapTerrainTileSupplier::onCleanupGraphics);
    }

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    m_tileComputeSharedDescriptorSet = DescriptorSet::create(s_tileComputeSharedDescriptorSetLayout, descriptorPool, "HeightmapTerrainTileSupplier-TileComputeSharedDescriptorSet");

    DescriptorSetWriter(m_tileComputeSharedDescriptorSet)
            .writeImage(0, s_heightRangeComputeSampler, m_heightmapImageView.get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
            .write();

    return true;
}


HeightmapTerrainTileSupplier::RequestTexture* HeightmapTerrainTileSupplier::createRequestTexture(const vk::CommandBuffer& commandBuffer, uint32_t width, uint32_t height, uint32_t mipLevels) {

    uint32_t requiredDescriptors = mipLevels;

    while (m_numUsedRequestTextureDescriptors + requiredDescriptors >= 128 && !m_availableRequestTextures.empty()) {
        // If there is not enough available descriptors to allocate one, delete the allocated textures until we run out, or enough descriptors become available.
        RequestTexture* requestTexture = m_availableRequestTextures.back();
        m_availableRequestTextures.pop_back();
        deleteRequestTexture(requestTexture);
    }

    if (m_numUsedRequestTextureDescriptors + requiredDescriptors >= 128) {
        return nullptr;
    }

    RequestTexture* requestTexture = new RequestTexture();

    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.width = width;
    imageConfig.height = width;
    imageConfig.mipLevels = mipLevels;
    imageConfig.format = vk::Format::eR32G32Sfloat;
    imageConfig.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
    requestTexture->heightRangeTempImage = Image2D::create(imageConfig, "HeightmapTerrainTileSupplier-HeightRangeComputeTempDownsampleImage");

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.image = requestTexture->heightRangeTempImage->getImage();
    imageViewConfig.mipLevelCount = 1;

    requestTexture->heightRangeTempImageViews.resize(mipLevels);

    for (uint32_t i = 0; i < mipLevels; ++i) {
        imageViewConfig.baseMipLevel = i;
        requestTexture->heightRangeTempImageViews[i] = ImageView::create(imageViewConfig, "HeightmapTerrainTileSupplier-HeightRangeComputeTempMipImageView");
    }

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1);
    ImageTransitionState dstState(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::PipelineStageFlagBits::eComputeShader);
    ImageUtil::transitionLayout(commandBuffer, requestTexture->heightRangeTempImage->getImage(), subresourceRange, ImageTransition::FromAny(vk::PipelineStageFlagBits::eComputeShader), dstState);

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();
    requestTexture->descriptorSet = DescriptorSet::create(s_tileComputeDescriptorSetLayout, descriptorPool, "HeightmapTerrainTileSupplier-TileComputeDescriptorSet");

    uint32_t maxArrayCount = 16;
    uint32_t arrayCount = glm::min(maxArrayCount, (uint32_t)requestTexture->heightRangeTempImageViews.size());

    DescriptorSetWriter writer(requestTexture->descriptorSet);
    writer.writeImage(0, s_heightRangeComputeSampler, requestTexture->heightRangeTempImageViews.data(), vk::ImageLayout::eGeneral, 0, arrayCount);
    writer.writeImage(1, s_heightRangeComputeSampler, requestTexture->heightRangeTempImageViews.data(), vk::ImageLayout::eGeneral, 0, arrayCount);
    if (arrayCount < maxArrayCount) {
        writer.writeImage(0, s_heightRangeComputeSampler, requestTexture->heightRangeTempImageViews[0], vk::ImageLayout::eGeneral, arrayCount, maxArrayCount - arrayCount);
        writer.writeImage(1, s_heightRangeComputeSampler, requestTexture->heightRangeTempImageViews[0], vk::ImageLayout::eGeneral, arrayCount, maxArrayCount - arrayCount);
    }
    writer.write();

    requestTexture->debugUsed = false;

    m_numUsedRequestTextureDescriptors += requiredDescriptors;
    return requestTexture;
}

void HeightmapTerrainTileSupplier::deleteRequestTexture(RequestTexture* requestTexture) {
    assert(!requestTexture->debugUsed);
    assert(requestTexture->fence == nullptr && !requestTexture->commandBuffer);

    delete requestTexture->descriptorSet;

    delete requestTexture->heightRangeTempImage;

    for (ImageView* imageView : requestTexture->heightRangeTempImageViews)
        delete imageView;

    m_numUsedRequestTextureDescriptors -= (uint32_t)requestTexture->heightRangeTempImageViews.size();

    delete requestTexture;
}

void HeightmapTerrainTileSupplier::makeRequestTextureAvailable(RequestTexture* requestTexture) {
    // Insert it into the available textures array at the sorted location. The smallest textures are at the front
    m_availableRequestTextures.insert(std::upper_bound(m_availableRequestTextures.begin(), m_availableRequestTextures.end(), requestTexture, [](const RequestTexture* lhs, const RequestTexture* rhs) {
        if (lhs->heightRangeTempImage->getWidth() != rhs->heightRangeTempImage->getWidth()) return lhs->heightRangeTempImage->getWidth() < rhs->heightRangeTempImage->getWidth();
        return lhs->heightRangeTempImage->getHeight() < rhs->heightRangeTempImage->getHeight();
    }), requestTexture);
}

bool HeightmapTerrainTileSupplier::updateTerrainTileHeightRangeDescriptors(const vk::CommandBuffer& commandBuffer, TileData* tileData) {

    glm::uvec2 tileCoordMin = getLowerTexelCoord(tileData->tileOffset);
    glm::uvec2 tileCoordMax = getUpperTexelCoords(tileData->tileOffset + tileData->tileSize);

    glm::uvec2 tileSize = tileCoordMax - tileCoordMin;

    TextureData& textureData = m_tileTextures[tileData->tileTextureIndex];

    glm::uvec2 imageSize = tileSize / 2u;
    uint32_t mipLevels = ImageUtil::getMaxMipLevels(imageSize.x, imageSize.y, 1);

    if (textureData.requestTexture != nullptr) {
        RequestTexture* requestTexture = textureData.requestTexture;

        if (glm::any(glm::lessThan(requestTexture->heightRangeTempImage->getSize(), imageSize)) || requestTexture->heightRangeTempImage->getMipLevelCount() < mipLevels) {
            // Exiting image is not big enough.
            textureData.requestTexture = nullptr;

            makeRequestTextureAvailable(requestTexture);
        }
    }

    if (textureData.requestTexture != nullptr) {
        return true;
    }

    // Find the smallest allocated texture that we can use for this tile.
    auto it = std::lower_bound(m_availableRequestTextures.begin(), m_availableRequestTextures.end(), imageSize, [](const RequestTexture* lhs, const glm::uvec2& rhs) {
        if (lhs->heightRangeTempImage->getWidth() != rhs.x) return lhs->heightRangeTempImage->getWidth() < rhs.x;
        return lhs->heightRangeTempImage->getHeight() < rhs.y;
    });

    size_t idx = std::distance(m_availableRequestTextures.begin(), it);

    if (it != m_availableRequestTextures.end()) {
        assert(it >= m_availableRequestTextures.begin() && it < m_availableRequestTextures.end());
        textureData.requestTexture = *it;
        m_availableRequestTextures.erase(it);
        assert(glm::all(glm::greaterThanEqual(textureData.requestTexture->heightRangeTempImage->getSize(), imageSize)) && textureData.requestTexture->heightRangeTempImage->getMipLevelCount() >= mipLevels);
    }
    if (textureData.requestTexture != nullptr) {
        return true;
    }

    // We still don't have a texture. We want to try to allocate one.

    textureData.requestTexture = createRequestTexture(commandBuffer, imageSize.x, imageSize.y, mipLevels);
    if (textureData.requestTexture != nullptr) {
        return true;
    }

    return false;
}

bool HeightmapTerrainTileSupplier::computeTerrainTileHeightRange(const vk::CommandBuffer& commandBuffer, TileData* tileData, Fence* fence) {

    assert(tileData->tileTextureIndex != UINT32_MAX);
    TextureData& textureData = m_tileTextures[tileData->tileTextureIndex];

    RequestTexture* requestTexture = textureData.requestTexture;
    assert(requestTexture != nullptr);

    assert(fence != nullptr && fence->getStatus() == Fence::Status_NotSignaled);

    assert(!requestTexture->debugUsed);
    assert(requestTexture->id == getTileId(tileData->tileOffset, tileData->tileSize));

    requestTexture->commandBuffer = commandBuffer;
    requestTexture->fence = fence;
    requestTexture->debugUsed = true;

    m_pendingTilesQueue.emplace_back(tileData);
    tileData->state = TileData::State_Pending;

    glm::uvec2 tileCoordMin = getLowerTexelCoord(tileData->tileOffset);
    glm::uvec2 tileCoordMax = getUpperTexelCoords(tileData->tileOffset + tileData->tileSize);

    ImageRegion region{
            .x = tileCoordMin.x,
            .y = tileCoordMin.y,
            .width = tileCoordMax.x - tileCoordMin.x,
            .height = tileCoordMax.y - tileCoordMin.y,
            .layerCount = 1,
            .mipLevelCount = 1
    };

    uint32_t imageWidth = region.width / 2;
    uint32_t imageHeight = region.height / 2;
    uint32_t mipLevels = ImageUtil::getMaxMipLevels(imageWidth, imageHeight, 1);

    constexpr uint32_t workgroupSize = 16;
    uint32_t workgroupCountX, workgroupCountY;

    TerrainTileHeightRangePushConstants heightRangeComputePushConstants{};
    heightRangeComputePushConstants.dstResolution = glm::uvec2(imageWidth, imageHeight);
    heightRangeComputePushConstants.tileOffset = tileCoordMin;

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    for (uint32_t i = 0; i < mipLevels; ++i) {

        heightRangeComputePushConstants.level = i;
        heightRangeComputePushConstants.srcImageIndex = glm::max(i, 1u) - 1;
        heightRangeComputePushConstants.dstImageIndex = i;

        s_heightRangeComputePipeline->bind(commandBuffer);
        const vk::PipelineLayout& clearPipelineLayout = s_heightRangeComputePipeline->getPipelineLayout();
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, clearPipelineLayout, 0, 1, &m_tileComputeSharedDescriptorSet->getDescriptorSet(), 0, nullptr);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, clearPipelineLayout, 1, 1, &requestTexture->descriptorSet->getDescriptorSet(), 0, nullptr);
        commandBuffer.pushConstants(clearPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(TerrainTileHeightRangePushConstants), &heightRangeComputePushConstants);
        workgroupCountX = INT_DIV_CEIL(heightRangeComputePushConstants.dstResolution.x, workgroupSize);
        workgroupCountY = INT_DIV_CEIL(heightRangeComputePushConstants.dstResolution.y, workgroupSize);
        s_heightRangeComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);

        heightRangeComputePushConstants.tileOffset = glm::uvec2(0u);
        heightRangeComputePushConstants.dstResolution >>= 1;
        heightRangeComputePushConstants.dstResolution = glm::max(heightRangeComputePushConstants.dstResolution, 1);

        // All shader writes for the previous mip level must be complete before we perform any reads from it for this mip level
        subresourceRange.baseMipLevel = i;
        ImageTransitionState srcState(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryWrite, vk::PipelineStageFlagBits::eComputeShader);
        ImageTransitionState dstState(vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryRead, vk::PipelineStageFlagBits::eComputeShader);
        ImageUtil::transitionLayout(commandBuffer, requestTexture->heightRangeTempImage->getImage(), subresourceRange, srcState, dstState);
    }


//    tileData->state = TileData::State_Pending;
//
//    size_t pixelCount = region.width * region.height;
//    float* pixels = new float[pixelCount];
//    m_heightmapImage->readPixels(pixels, ImagePixelLayout::R, ImagePixelFormat::Float32, vk::ImageAspectFlagBits::eColor, region, ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader));
//
//    float minElevation = +INFINITY;
//    float maxElevation = -INFINITY;
//
//    for (uint32_t i = 0; i < pixelCount; ++i) {
//        minElevation = glm::min(minElevation, pixels[i]);
//        maxElevation = glm::max(maxElevation, pixels[i]);
//    }
//    tileData->heightData = pixels;
//    tileData->minHeight = minElevation;
//    tileData->maxHeight = maxElevation;
//
//    tileData->state = TileData::State_Available;

    return true;
}

std::pair<vk::CommandBuffer, Fence*> HeightmapTerrainTileSupplier::getComputeCommandBuffer() {
    if (!m_availableCommandBuffers.empty()) {
        auto commandBuffer = m_availableCommandBuffers.back();
        m_availableCommandBuffers.pop_back();
        assert(commandBuffer.first && commandBuffer.second != nullptr);
        return commandBuffer;
    } else {
        FenceConfiguration fenceConfig{};
        fenceConfig.device = Engine::graphics()->getDevice();
        Fence* fence = Fence::create(fenceConfig, "TerrainTileSupplier-PendingTileFence");

        CommandBufferConfiguration commandBufferConfig{};
        commandBufferConfig.level = vk::CommandBufferLevel::ePrimary;
        vk::CommandBuffer commandBuffer = **Engine::graphics()->commandPool()->allocateCommandBuffer(commandBufferConfig, "terrain_compute_buffer");

        return std::make_pair(commandBuffer, fence);
    }
}