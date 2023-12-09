#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"


QuadtreeTerrainComponent::QuadtreeTerrainComponent():
        m_tileQuadtree(new TerrainTileQuadtree(8, glm::dvec2(1000.0, 1000.0), 100.0)),
        m_tileResolution(64),
        m_tileGridSize(16) {
}

QuadtreeTerrainComponent::~QuadtreeTerrainComponent() {
//    delete m_tileQuadtree;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth) {
    m_tileQuadtree->setMaxQuadtreeDepth(maxQuadtreeDepth);
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setTileResolution(uint32_t tileResolution) {
    m_tileResolution = tileResolution;
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setTileGridSize(uint32_t tileGridSize) {
    if (tileGridSize < 1)
        tileGridSize = 1;
    m_tileGridSize = tileGridSize;
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setSize(const glm::dvec2& size) {
    m_tileQuadtree->setSize(size);
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setHeightScale(double heightScale) {
    m_tileQuadtree->setHeightScale(heightScale);
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setTileSupplier(const std::shared_ptr<TerrainTileSupplier>& tileSupplier) {
    m_tileQuadtree->setTileSupplier(tileSupplier);
    return *this;
}

uint32_t QuadtreeTerrainComponent::getMaxQuadtreeDepth() const {
    return m_tileQuadtree->getMaxQuadtreeDepth();
}

uint32_t QuadtreeTerrainComponent::getTileResolution() const {
    return m_tileResolution;
}

uint32_t QuadtreeTerrainComponent::getTileGridSize() const {
    return m_tileGridSize;
}

const glm::dvec2& QuadtreeTerrainComponent::getSize() const {
    return m_tileQuadtree->getSize();
}

double QuadtreeTerrainComponent::getHeightScale() const {
    return m_tileQuadtree->getHeightScale();
}

const std::shared_ptr<TerrainTileSupplier>& QuadtreeTerrainComponent::getTileSupplier() const {
    return m_tileQuadtree->getTileSupplier();
}

const std::unique_ptr<TerrainTileQuadtree>& QuadtreeTerrainComponent::getTileQuadtree() const {
    return m_tileQuadtree;
}