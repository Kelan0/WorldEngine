
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/DescriptorSet.h"
#include "core/util/Logger.h"

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
        priority(UINT32_MAX),
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

void TileDataReference::setPriority(float priority) {
    assert(valid());
    m_tileData->priority = priority;
}

float TileDataReference::getPriority() const {
    assert(valid());
    return m_tileData->priority;
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








