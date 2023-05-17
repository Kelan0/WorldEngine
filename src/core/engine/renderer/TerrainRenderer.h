#ifndef WORLDENGINE_TERRAINRENDERER_H
#define WORLDENGINE_TERRAINRENDERER_H

#include "core/core.h"

class Frustum;

class TerrainRenderer {
public:
    TerrainRenderer();

    ~TerrainRenderer();

    bool init();

    void preRender(double dt);

    void render(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum);

private:

};


#endif //WORLDENGINE_TERRAINRENDERER_H
