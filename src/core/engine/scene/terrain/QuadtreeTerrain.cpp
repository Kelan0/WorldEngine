//
// Created by Kelan on 19/05/2023.
//

#include "QuadtreeTerrain.h"

QuadtreeTerrainComponent::QuadtreeTerrainComponent():
    m_maxQuadtreeDepth(6),
    m_tileResolution(64),
    m_size(1000.0, 1000.0),
    m_heightScale(100.0) {

}

QuadtreeTerrainComponent::~QuadtreeTerrainComponent() {

}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setMaxQuadtreeDepth(uint32_t maxQuadtreeDepth) {
    m_maxQuadtreeDepth = maxQuadtreeDepth;
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setTileResolution(uint32_t tileResolution) {
    m_tileResolution = tileResolution;
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setSize(const glm::dvec2& size) {
    m_size = size;
    return *this;
}

QuadtreeTerrainComponent& QuadtreeTerrainComponent::setHeightScale(double heightScale) {
    m_heightScale = heightScale;
    return *this;
}

uint32_t QuadtreeTerrainComponent::getMaxQuadtreeDepth() const {
    return m_maxQuadtreeDepth;
}

uint32_t QuadtreeTerrainComponent::getTileResolution() const {
    return m_tileResolution;
}

const glm::dvec2& QuadtreeTerrainComponent::getSize() const {
    return m_size;
}

double QuadtreeTerrainComponent::getHeightScale() const {
    return m_heightScale;
}

TerrainTileQuadtree* QuadtreeTerrainComponent::getTileQuadtree() const {
    return m_tileQuadtree;
}