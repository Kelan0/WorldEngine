#ifndef WORLDENGINE_TERRAINRENDERER_H
#define WORLDENGINE_TERRAINRENDERER_H

#include "core/core.h"
#include "core/engine/scene/Scene.h"

class Mesh;
class Frustum;
class Transform;
class QuadtreeTerrainComponent;

class TerrainRenderer {
public:
    TerrainRenderer();

    ~TerrainRenderer();

    bool init();

    void preRender(double dt);

    void drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum);

    void setScene(Scene* scene);

    Scene* getScene() const;

private:
    void drawQuadtreeTerrain(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum);

private:
    Scene* m_scene;
    std::shared_ptr<Mesh> m_terrainTileMesh;
};


#endif //WORLDENGINE_TERRAINRENDERER_H
