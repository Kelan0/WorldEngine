#ifndef WORLDENGINE_TERRAINRENDERER_H
#define WORLDENGINE_TERRAINRENDERER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
#include "core/graphics/FrameResource.h"

class Mesh;
class Frustum;
class Transform;
class QuadtreeTerrainComponent;
class Buffer;
class Texture;
class Sampler;
class Image2D;
class ImageView;
class DescriptorSet;
class DescriptorSetLayout;
class Scene;

class TerrainRenderer {
private:
    struct GPUTerrainTileData {
        glm::vec2 tilePosition;
        glm::vec2 tileSize;
        glm::vec2 textureOffset;
        glm::vec2 textureSize;
    };

    struct GPUTerrainUniformData {
        glm::mat4 terrainTransformMatrix;
        glm::vec4 terrainScale;
        uint32_t heightmapTextureIndex;
        uint32_t tileGridSize;
        uint32_t _pad0[2 + 4 + 4];
    };

    struct RenderResources {
        Buffer* terrainUniformBuffer;
        Buffer* terrainTileDataBuffer;
        DescriptorSet* terrainDescriptorSet;
    };

    struct VisibilityIndices {
        uint32_t firstInstance;
        uint32_t instanceCount;
    };

public:
    struct InstanceInfo {
        uint32_t firstInstance;
        uint32_t instanceCount;
    };

public:

    TerrainRenderer();

    ~TerrainRenderer();

    bool init();

    void preRender(double dt);

    void renderGeometryPass(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex);

    void renderShadowPass(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex);

    void resetVisibility();

    void applyVisibility();

    uint32_t updateVisibility(double dt, const RenderCamera* renderCamera, const Frustum* frustum);

    void drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex);

    void recordRenderCommands(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex);

    void setScene(Scene* scene);

    Scene* getScene() const;

    const SharedResource<DescriptorSetLayout>& getTerrainDescriptorSetLayout() const;

    DescriptorSet* getTerrainDescriptorSet() const;

    static uint32_t getTileVertexCount(uint32_t tileSize);

    const std::vector<InstanceInfo>& getGlobalTerrainInstances() const;

private:
    void updateQuadtreeTerrainTiles(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const Frustum* frustum);

    void* mapTerrainTileDataBuffer(size_t maxObjects);

    void* mapTerrainUniformBuffer(size_t maxObjects);

    void initializeDefaultEmptyHeightmapTexture();

private:
    Scene* m_scene;
    FrameResource<RenderResources> m_resources;

    SharedResource<DescriptorSetLayout> m_terrainDescriptorSetLayout;

    std::vector<VisibilityIndices> m_visibilityIndices;
    std::shared_ptr<Mesh> m_terrainTileMesh;
    std::vector<GPUTerrainTileData> m_terrainTileDataBuffer;
    std::vector<GPUTerrainUniformData> m_terrainUniformData;
    std::vector<InstanceInfo> m_globalTerrainInstances;
    std::vector<ImageView*> m_heightmapImageViews;

    std::shared_ptr<Sampler> m_defaultHeightmapSampler;
    std::shared_ptr<Image2D> m_defaultEmptyHeightmapImage;
    std::shared_ptr<ImageView> m_defaultEmptyHeightmapImageView;
    bool m_visibilityApplied;

    std::map<RenderCamera, size_t> m_viewInstanceIndices;
};


#endif //WORLDENGINE_TERRAINRENDERER_H
