
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"

TileData::TileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize):
        referenceCount(0),
        tileTextureIndex(UINT32_MAX),
        tileOffset(tileOffset),
        tileSize(tileSize),
        minHeight(0.0),
        maxHeight(1.0),
        heightData(nullptr),
        requested(false),
        pending(false),
        available(false) {
}

TileData::~TileData() {

}



TileDataReference::TileDataReference(TileData* tileData) :
    m_tileData(tileData) {
    if (m_tileData != nullptr)
        ++m_tileData->referenceCount;
}

TileDataReference::TileDataReference(const TileDataReference& copy) :
    m_tileData(copy.m_tileData) {
    if (m_tileData != nullptr)
        ++m_tileData->referenceCount;
}

TileDataReference::TileDataReference(TileDataReference&& move) noexcept :
    m_tileData(std::exchange(move.m_tileData, nullptr)) {
}

TileDataReference::~TileDataReference() {
    if (m_tileData != nullptr) {
        assert(m_tileData->referenceCount > 0);
        --m_tileData->referenceCount;
    }
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
    return m_tileData == nullptr;
}

bool TileDataReference::operator!=(std::nullptr_t) const {
    return m_tileData != nullptr;
}

bool TileDataReference::operator==(const TileDataReference& rhs) const {
    return m_tileData == rhs.m_tileData;
}

bool TileDataReference::operator!=(const TileDataReference& rhs) const {
    return m_tileData != rhs.m_tileData;
}

const glm::dvec2& TileDataReference::getTileOffset() const {
    return m_tileData->tileOffset;
}

const glm::dvec2& TileDataReference::getTileSize() const {
    return m_tileData->tileSize;
}

float TileDataReference::getMinHeight() const {
    return m_tileData->minHeight;
}

float TileDataReference::getMaxHeight() const {
    return m_tileData->maxHeight;
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

void HeightmapTerrainTileSupplier::update() const {

}

const std::vector<ImageView*>& HeightmapTerrainTileSupplier::getLoadedTileImageViews() const {
    return m_loadedTileImageViews;
}

uint32_t HeightmapTerrainTileSupplier::requestTileData(const glm::dvec2& tileOffset, const glm::dvec2& tileSize) {
    glm::uvec2 texelBoundMin = getLowerTexelCoord(tileOffset);
    glm::uvec2 texelBoundMax = getUpperTexelCoords(tileOffset + tileSize);
    glm::uvec4 texelBounds(texelBoundMin, texelBoundMax);

    uint32_t id;

    TileData* tileData;

    auto it = m_tileBounds.find(texelBounds);

    if (it != m_tileBounds.end()) {
        id = it->second;
        tileData = m_loadedTiles.at(id);

    } else {
        id = m_idManager.getID();
        m_tileBounds.insert(std::make_pair(texelBounds, id));

        auto result = m_loadedTiles.insert(std::make_pair(id, new TileData(tileOffset, tileSize)));
        assert(result.second && "Tile ID conflict");

        tileData = result.first->second;
    }

    if (!tileData->available) {
        tileData->requested = true;
    }
    ++tileData->referenceCount;
    return id;
}

void HeightmapTerrainTileSupplier::releaseTileData(uint32_t id) {
    auto it = m_loadedTiles.find(id);
    if (it == m_loadedTiles.end()) {
        return;
    }

    TileData* tileData = it->second;
    --tileData->referenceCount;

    if (tileData->referenceCount == 0) {
        glm::uvec2 texelBoundMin = getLowerTexelCoord(tileData->tileOffset);
        glm::uvec2 texelBoundMax = getUpperTexelCoords(tileData->tileOffset + tileData->tileSize);
        glm::uvec4 texelBounds(texelBoundMin, texelBoundMax);
        m_loadedTiles.erase(it);
        m_tileBounds.erase(texelBounds);
        m_idManager.freeID(id);
    }
}


TileDataReference HeightmapTerrainTileSupplier::getTile(uint32_t id) const {
    auto it = m_loadedTiles.find(id);
    if (it == m_loadedTiles.end()) {
        return TileDataReference(nullptr);
    }

    return TileDataReference(it->second);
}

const std::shared_ptr<Image2D>& HeightmapTerrainTileSupplier::getHeightmapImage() const {
    return m_heightmapImage;
}

const std::shared_ptr<ImageView>& HeightmapTerrainTileSupplier::getHeightmapImageView() const {
    return m_heightmapImageView;
}

glm::uvec2 HeightmapTerrainTileSupplier::getLowerTexelCoord(const glm::uvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(0);
    return glm::uvec2((uint32_t)glm::floor((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::floor((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}

glm::uvec2 HeightmapTerrainTileSupplier::getUpperTexelCoords(const glm::uvec2& normalizedCoord) const {
    if (m_heightmapImage == nullptr)
        return glm::uvec2(1);
    return glm::uvec2((uint32_t)glm::ceil((double)m_heightmapImage->getWidth() * normalizedCoord.x),
                      (uint32_t)glm::ceil((double)m_heightmapImage->getHeight() * normalizedCoord.y));
}