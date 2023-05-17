#ifndef WORLDENGINE_TERRAINCOMPONENT_H
#define WORLDENGINE_TERRAINCOMPONENT_H

#include "core/core.h"

class TerrainComponent {
public:
    TerrainComponent();

    ~TerrainComponent();

    TerrainComponent& setResolution(uint32_t resolution);

    TerrainComponent& setSize(const glm::dvec2& size);

    uint32_t getResolution() const;

    const glm::dvec2& getSize() const;

private:
    uint32_t m_resolution;
    glm::dvec2 m_size;
};


#endif //WORLDENGINE_TERRAINCOMPONENT_H
