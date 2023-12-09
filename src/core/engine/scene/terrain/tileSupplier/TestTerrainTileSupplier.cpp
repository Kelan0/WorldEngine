#include "TestTerrainTileSupplier.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/Logger.h"

TestTerrainTileSupplier::TestTerrainTileSupplier(ImageData* heightmapImageData):
    TerrainTileSupplier(),
    m_heightmapImageData(heightmapImageData),
    m_heightmapImageView(nullptr) {

    float minValue = +INFINITY;
    float maxValue = -INFINITY;
    std::map<int32_t, double> counts;
    for (int y = 0; y < heightmapImageData->getHeight(); ++y) {
        for (int x = 0; x < heightmapImageData->getWidth(); ++x) {
            float f = (float)heightmapImageData->getChannelf(x, y, 0);
            minValue = glm::min(minValue, f);
            maxValue = glm::max(maxValue, f);
//            f = glm::clamp(f, 0.0, 1.0);
//            uint32_t h = (uint32_t)(f * UINT32_MAX);
            int32_t h = (int32_t)(f * 25600);
            ++counts[h];
        }
    }

    LOG_INFO("Heightmap has %zu unique heights, max=%f, min=%f\n", counts.size(), maxValue, minValue);


    Image2DConfiguration heightmapImageConfig{};
    heightmapImageConfig.device = Engine::graphics()->getDevice();
    heightmapImageConfig.imageData = heightmapImageData;
    heightmapImageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
//    heightmapImageConfig.format = vk::Format::eR32Sfloat;
    heightmapImageConfig.format = vk::Format::eR32G32B32A32Sfloat;
    heightmapImageConfig.mipLevels = UINT32_MAX;
    m_heightmapImage = std::shared_ptr<Image2D>(Image2D::create(heightmapImageConfig, "HeightmapTerrainTileSupplier-TerrainHeightmapImage"));

    ImageViewConfiguration heightmapImageViewConfig{};
    heightmapImageViewConfig.device = Engine::graphics()->getDevice();
    heightmapImageViewConfig.format = heightmapImageConfig.format;
    heightmapImageViewConfig.setImage(m_heightmapImage.get());
    m_heightmapImageView = std::shared_ptr<ImageView>(ImageView::create(heightmapImageViewConfig, "HeightmapTerrainTileSupplier-TerrainHeightmapImageView"));

    m_loadedTileImageViews.emplace_back(m_heightmapImageView.get());

    m_tileIdleTimeoutSeconds = 10.0F; // 30 seconds
    m_tileExpireTimeoutSeconds = 30.0F; // 3 minutes
}

TestTerrainTileSupplier::~TestTerrainTileSupplier() {
    
}

void TestTerrainTileSupplier::update() {
    auto now = Time::now();
    uint64_t tileIdleTimeoutNanoseconds = (uint64_t)(m_tileIdleTimeoutSeconds * 1e+9);
    uint64_t tileExpireTimeoutNanoseconds = (uint64_t)(m_tileExpireTimeoutSeconds * 1e+9);

    // Deallocate tiles that have been idle for too long.
    for (auto it = m_idleTiles.begin(); it != m_idleTiles.end();) {
        glm::uvec4 id = it->first;
        TileData* tileData = it->second;

        if (Time::nanoseconds(tileData->timeLastUsed, now) <= tileExpireTimeoutNanoseconds) {
            ++it;
            continue; // Tile has not expired...
        }

        if (!tileData->deleted) {
            tileData->deleted = true;
        }

//        if (tileData->tileTextureIndex != UINT32_MAX) {
//            assert(tileData->tileTextureIndex < m_tileTextures.size());
//            m_availableTileTextureIndices.emplace_back(tileData->tileTextureIndex);
//        }
        TileDataReference::invalidateAllReferences(tileData);
        it = m_idleTiles.erase(it);
    }


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
            tileData->state = TileData::State_Pending;
            m_pendingTilesQueue.emplace_back(tileData);
        }

        ++it;
    }

    std::sort(m_pendingTilesQueue.begin(), m_pendingTilesQueue.end(), [](const TileData* lhs, const TileData* rhs) {
        return lhs->priority < rhs->priority; // Sort the high priority tiles to the end of the array, since they are popped off first.
    });

    size_t x = 0;

    while (!m_pendingTilesQueue.empty()) {
        TileData* tileData = m_pendingTilesQueue.back();
        m_pendingTilesQueue.pop_back();

        computeTerrainTileHeightRange(tileData);

        if (++x > 8) {
            break;
        }
//        break;
    }


//    if (didBeginThreadBatch) {
//        ThreadUtils::endBatch();
//    }
}

const std::vector<ImageView*>& TestTerrainTileSupplier::getLoadedTileImageViews() const {
    return m_loadedTileImageViews;
}

TileDataReference TestTerrainTileSupplier::getTile(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) {
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
    bool wasInserted = result.second;
    assert(wasInserted && "Tile ID conflict");

    tileData = result.first->second;
    tileData->idle = false;
    tileData->referenceCount = 1;

    requestTileData(tileData);
    return TileDataReference(tileData);
}

TestTerrainTileSupplier::TileIterator TestTerrainTileSupplier::markIdle(const TileIterator& it) {
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

TestTerrainTileSupplier::TileIterator TestTerrainTileSupplier::markActive(const TileIterator& it) {
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

void TestTerrainTileSupplier::requestTileData(TileData* tileData) {
    if (tileData->state == TileData::State_None) {
        tileData->state = TileData::State_Requested;
        tileData->timeRequested = Time::now();
    }
}

glm::uvec2 TestTerrainTileSupplier::getTileTextureSize(const glm::dvec2& normalizedSize) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(0);
    return glm::uvec2((uint32_t)glm::ceil((double)m_heightmapImage->getWidth() * normalizedSize.x),
                      (uint32_t)glm::ceil((double)m_heightmapImage->getHeight() * normalizedSize.y));
}

glm::uvec2 TestTerrainTileSupplier::getLowerTexelCoord(const glm::dvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(0);
    return glm::uvec2((uint32_t)glm::floor((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::floor((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}

glm::uvec2 TestTerrainTileSupplier::getUpperTexelCoords(const glm::dvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(1);
    return glm::uvec2((uint32_t)glm::ceil((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::ceil((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}

glm::uvec4 TestTerrainTileSupplier::getTileId(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) const {
    glm::uvec2 texelBoundMin = getLowerTexelCoord(tileOffset);
    glm::uvec2 texelBoundMax = getUpperTexelCoords(tileOffset + tileSize);
    return glm::uvec4(texelBoundMin, texelBoundMax);
}

void TestTerrainTileSupplier::computeTerrainTileHeightRange(TileData* tileData) {
    auto t0 = Time::now();
    glm::uvec2 minCoord = getLowerTexelCoord(tileData->tileOffset);
    glm::uvec2 maxCoord = getUpperTexelCoords(tileData->tileOffset + tileData->tileSize);
//    LOG_INFO("Generating tile [%u,%u : %ux%u]", minCoord.x, minCoord.y, maxCoord.x - minCoord.x, maxCoord.y - minCoord.y);

    float minHeight = INFINITY;
    float maxHeight = -INFINITY;

    uint32_t x_incr = 1;//glm::max(1u, (maxCoord.x - minCoord.x) / 1024);
    uint32_t y_incr = 1;//glm::max(1u, (maxCoord.y - minCoord.y) / 1024);

    for (uint32_t y = minCoord.y; y < maxCoord.y; y += y_incr) {
        for (uint32_t x = minCoord.x; x < maxCoord.x; x += x_incr) {
            float height = m_heightmapImageData->getChannelf(x, y, 0);

            minHeight = glm::min(minHeight, height);
            maxHeight = glm::max(maxHeight, height);
        }
    }

    tileData->minHeight = minHeight;
    tileData->maxHeight = maxHeight;
    tileData->state = TileData::State_Available;
//    LOG_INFO("Took %.2f msec to generate tile [%u,%u : %ux%u]", Time::milliseconds(t0), minCoord.x, minCoord.y, maxCoord.x - minCoord.x, maxCoord.y - minCoord.y);
}

void TestTerrainTileSupplier::computeTerrainTileMipLevel(glm::uvec2 minCoord, glm::uvec2 maxCoord, uint32_t level) {
    if (level == 0) {
        return; // Nothing to do at the base mip level
    }

    minCoord = minCoord >> level;
    maxCoord = (maxCoord + glm::uvec2(1)) >> level;

    for (uint32_t y = minCoord.y; y < maxCoord.y; ++y) {
        uint32_t y1 = y >> 1;

        for (uint32_t x = minCoord.x; x < maxCoord.x; ++x) {
            uint32_t x1 = x >> 1;


        }
    }
}