#ifndef WORLDENGINE_TERRAINRENDERER_H
#define WORLDENGINE_TERRAINRENDERER_H

#include "core/core.h"
#include "core/engine/scene/Scene.h"

class Mesh;
class Frustum;
class Transform;
class QuadtreeTerrainComponent;
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;

class TerrainRenderer {
public:
    TerrainRenderer();

    ~TerrainRenderer();

    bool init();

    void preRender(double dt);

    void drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum);

    void setScene(Scene* scene);

    Scene* getScene() const;

    const SharedResource<DescriptorSetLayout>& getTerrainDescriptorSetLayout() const;

    DescriptorSet* getTerrainDescriptorSet() const;

private:
    void updateQuadtreeTerrainTiles(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const Frustum* frustum);

    void* mapTerrainDataBuffer(size_t maxObjects);

private:
    struct GPUTerrainData {
        glm::mat4 modelMatrix;
    };

    struct RenderResources {
        Buffer* terrainDataBuffer;
        DescriptorSet* terrainDescriptorSet;
    };

private:
    Scene* m_scene;
    FrameResource<RenderResources> m_resources;

    SharedResource<DescriptorSetLayout> m_terrainDescriptorSetLayout;

    std::shared_ptr<Mesh> m_terrainTileMesh;
    std::vector<GPUTerrainData> m_terrainDataBuffer;

};


#endif //WORLDENGINE_TERRAINRENDERER_H
