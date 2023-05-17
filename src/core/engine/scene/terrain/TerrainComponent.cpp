#include "TerrainComponent.h"

TerrainComponent::TerrainComponent():
    m_resolution(256),
    m_size(64.0){

}

TerrainComponent::~TerrainComponent() {

}

TerrainComponent& TerrainComponent::setResolution(uint32_t resolution) {
    m_resolution = resolution;
    return *this;
}

TerrainComponent& TerrainComponent::setSize(const glm::dvec2& size) {
    m_size = size;
    return *this;
}

uint32_t TerrainComponent::getResolution() const {
    return m_resolution;
}

const glm::dvec2& TerrainComponent::getSize() const {
    return m_size;
}