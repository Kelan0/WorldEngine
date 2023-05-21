#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"


QuadtreeTerrainComponent::QuadtreeTerrainComponent():
        m_tileQuadtree(new TerrainTileQuadtree(6, glm::dvec2(1000.0, 1000.0), 100.0)),
        m_tileResolution(64) {
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

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setSize(const glm::dvec2& size) {
    m_tileQuadtree->setSize(size);
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setHeightScale(double heightScale) {
    m_tileQuadtree->setHeightScale(heightScale);
    return *this;
}

uint32_t QuadtreeTerrainComponent::getMaxQuadtreeDepth() const {
    return m_tileQuadtree->getMaxQuadtreeDepth();
}

uint32_t QuadtreeTerrainComponent::getTileResolution() const {
    return m_tileResolution;
}

const glm::dvec2& QuadtreeTerrainComponent::getSize() const {
    return m_tileQuadtree->getSize();
}

double QuadtreeTerrainComponent::getHeightScale() const {
    return m_tileQuadtree->getHeightScale();
}

const std::unique_ptr<TerrainTileQuadtree>& QuadtreeTerrainComponent::getTileQuadtree() const {
    return m_tileQuadtree;
}