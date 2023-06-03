
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/util/Logger.h"

//size_t allocCounter = 0;

glm::uvec4 debugGetTileId(const glm::dvec2& offset, const glm::dvec2& size) {
    return glm::uvec4((uint32_t)glm::floor(8192.0 * offset.x),
                      (uint32_t)glm::floor(8192.0 * offset.y),
                      (uint32_t)glm::ceil(8192.0 * (offset.x + size.x)),
                      (uint32_t)glm::ceil(8192.0 * (offset.y + size.y)));
}

TileData::TileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) :
        referenceCount(0),
        tileTextureIndex(UINT32_MAX),
        tileOffset(tileOffset),
        tileSize(tileSize),
        minHeight(0.0),
        maxHeight(1.0),
        heightDataResolution(0, 0),
        heightData(nullptr),
        timeLastUsed(Time::now()),
        cameraDistance(INFINITY),
        requested(false),
        pending(false),
        available(false),
        idle(false) {
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
    return valid() && m_tileData->available;
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







HeightmapTerrainTileSupplier::HeightmapTerrainTileSupplier(const std::shared_ptr<Image2D>& heightmapImage) :
        TerrainTileSupplier(),
        m_heightmapImage(heightmapImage),
        m_heightmapImageView(nullptr) {

    if (m_heightmapImage) {
        ImageViewConfiguration heightmapImageViewConfig{};
        heightmapImageViewConfig.device = heightmapImage->getDevice();
        heightmapImageViewConfig.format = heightmapImage->getFormat();
        heightmapImageViewConfig.setImage(heightmapImage.get());
        m_heightmapImageView = std::shared_ptr<ImageView>(ImageView::create(heightmapImageViewConfig, heightmapImage->getName() + "-ImageView"));

        m_loadedTileImageViews.emplace_back(m_heightmapImageView.get());
    }
}

HeightmapTerrainTileSupplier::~HeightmapTerrainTileSupplier() {
}

size_t lastNodesUsedCount = 0;

void HeightmapTerrainTileSupplier::update() {

    std::vector<TileData*> expiredTiles;

    auto now = Time::now();
    uint64_t tileIdleTimeoutNanoseconds = (uint64_t)(10 * 1e+9); // 30 seconds
    uint64_t tileExpireTimeoutNanoseconds = (uint64_t)(30 * 1e+9); // 3 minutes

    size_t nodesUsedCount = 0;

    size_t numMadeIdle = 0;

    for (auto it = m_activeTiles.begin(); it != m_activeTiles.end();) {
        glm::uvec4 id = it->first;
        TileData* tileData = it->second;

        assert(tileData->referenceCount > 0 && !tileData->deleted && !tileData->idle);

        if (Time::nanoseconds(tileData->timeLastUsed, now) > tileIdleTimeoutNanoseconds) {
            it = markIdle(it);
            ++numMadeIdle;
            continue;
        } else if (Time::nanoseconds(tileData->timeLastUsed, now) < 1000000 * 100) {
            ++nodesUsedCount;
        }

        ++it;
    }

    size_t numDeallocated = 0;

    for (auto it = m_idleTiles.begin(); it != m_idleTiles.end();) {
        glm::uvec4 id = it->first;
        TileData* tileData = it->second;

        bool isDeleted = tileData->deleted;

        if (!isDeleted && Time::nanoseconds(tileData->timeLastUsed, now) > tileExpireTimeoutNanoseconds) {
            isDeleted = true;
            ++numDeallocated;
            TileDataReference::invalidateAllReferences(tileData);
        }

        if (isDeleted) {
            it = m_idleTiles.erase(it);
        } else {
            ++it;
        }
    }

//    if (numDeallocated > 0 || numMadeIdle > 0 || nodesUsedCount != lastNodesUsedCount) {
//        if (nodesUsedCount != lastNodesUsedCount)
//            lastNodesUsedCount = nodesUsedCount;
//        LOG_DEBUG("%zu tiles made idle, %zu tiles expired, %llu nodes in use", numMadeIdle, numDeallocated, nodesUsedCount);
//    }
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
    auto result = m_activeTiles.insert(std::make_pair(id, new TileData(tileOffset, tileSize)));
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
    tile->requested = false;
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

    tile->idle = false;
    ++tile->referenceCount; // Hold a fake reference to keep this tile alive while it's active.

//    LOG_DEBUG("Tile [%u %u, %u %u] became active again", id.x, id.y, id.z, id.w);
    tile->timeLastUsed = Time::now();

    auto it1 = m_idleTiles.erase(it);
    m_activeTiles.insert(std::make_pair(id, tile));
    return it1;
}

void HeightmapTerrainTileSupplier::requestTileData(TileData* tileData) {
    if (!tileData->available && !tileData->pending)
        tileData->requested = true;
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