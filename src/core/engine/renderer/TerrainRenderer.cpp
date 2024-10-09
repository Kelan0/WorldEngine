
#include "TerrainRenderer.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"
#include "core/engine/scene/terrain/TerrainTileSupplier.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/Material.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/util/Logger.h"
#include "RenderComponent.h"

#define TERRAIN_UNIFORM_BUFFER_BINDING 0
#define TERRAIN_TILE_DATA_BUFFER_BINDING 1
#define TERRAIN_HEIGHTMAP_TEXTURES_BINDING 2

const uint32_t tileGridSize = 16;

TerrainRenderer::TerrainRenderer() :
        m_scene(nullptr),
        m_terrainTileMesh(nullptr) {
}

TerrainRenderer::~TerrainRenderer() {
    LOG_INFO("Destroying TerrainRenderer");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        if (m_resources[i] != nullptr) {
            delete m_resources[i]->terrainTileDataBuffer;
            delete m_resources[i]->terrainUniformBuffer;
            delete m_resources[i]->terrainDescriptorSet;
        }
    }
}

bool TerrainRenderer::init() {
    LOG_INFO("Initializing TerrainRenderer");

    initializeDefaultEmptyHeightmapTexture();

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    m_terrainDescriptorSetLayout = builder
            .addUniformBuffer(TERRAIN_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, true) // global terrain uniform buffer
            .addStorageBuffer(TERRAIN_TILE_DATA_BUFFER_BINDING, vk::ShaderStageFlagBits::eVertex, false) // terrain tile data buffer
            .addCombinedImageSampler(TERRAIN_HEIGHTMAP_TEXTURES_BINDING, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 100) // terrain heightmap textures
            .build("TerrainRenderer-TerrainDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->terrainDescriptorSet = DescriptorSet::create(m_terrainDescriptorSetLayout, descriptorPool, "TerrainRenderer-TerrainDescriptorSetLayout");
        m_resources[i]->terrainTileDataBuffer = nullptr;
        m_resources[i]->terrainUniformBuffer = nullptr;

        DescriptorSetWriter(m_resources[i]->terrainDescriptorSet)
                .writeImage(TERRAIN_HEIGHTMAP_TEXTURES_BINDING, m_defaultHeightmapSampler.get(), m_defaultEmptyHeightmapImageView.get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 100)
                .write();
    }

    MeshData<Vertex> terrainTileMeshData;
    terrainTileMeshData.createPlane(glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec2(1.0F), glm::uvec2(15, 15));
    terrainTileMeshData.computeTangents();

    MaterialConfiguration terrainTileMaterialConfig{};
    terrainTileMaterialConfig.device = Engine::graphics()->getDevice();
    terrainTileMaterialConfig.setAlbedo(glm::vec3(0.1F, 0.25F, 0.65F));
    std::shared_ptr<Material> terrainTileMaterial = std::shared_ptr<Material>(Material::create(terrainTileMaterialConfig, "Test-TerrainTileMaterial"));

    MeshConfiguration terrainTileMeshConfig{};
    terrainTileMeshConfig.device = Engine::graphics()->getDevice();
    terrainTileMeshConfig.setMeshData(&terrainTileMeshData);
    m_terrainTileMesh = std::shared_ptr<Mesh>(Mesh::create(terrainTileMeshConfig, "TerrainRenderer-TerrainTileMesh"));

    return true;
}

void TerrainRenderer::preRender(double dt) {
    PROFILE_SCOPE("TerrainRenderer::preRender")

    resetVisibility();
}

void TerrainRenderer::renderGeometryPass(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex) {
    PROFILE_SCOPE("TerrainRenderer::renderGeometryPass")

//    updateVisibility(dt, renderCamera, frustum);

    if (m_globalTerrainInstances.empty()) {
        return;
    }

    auto graphicsPipeline = Engine::instance()->getDeferredRenderer()->getTerrainGeometryGraphicsPipeline();
    graphicsPipeline->bind(commandBuffer);
    drawTerrain(dt, commandBuffer, visibilityIndex);
}

void TerrainRenderer::renderShadowPass(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex) {
    PROFILE_SCOPE("TerrainRenderer::renderShadowPass")

    if (m_globalTerrainInstances.empty()) {
        return;
    }
}

void TerrainRenderer::resetVisibility() {

    m_terrainTileDataBuffer.clear();
    m_terrainUniformData.clear();
    m_globalTerrainInstances.clear();
    m_heightmapImageViews.clear();
    m_visibilityIndices.clear();
    m_visibilityApplied = false;

    const auto& terrainEntities = m_scene->registry()->view<QuadtreeTerrainComponent>();

    Transform identityTransform = Transform();

    for (auto it = terrainEntities.begin(); it != terrainEntities.end(); ++it) {
        Entity entity(m_scene, *it);

        const QuadtreeTerrainComponent& quadtreeTerrain = terrainEntities.get<QuadtreeTerrainComponent>(*it);
        const Transform* transform = entity.tryGetComponent<Transform>();
        if (transform == nullptr)
            transform = &identityTransform;

        GPUTerrainUniformData uniformData{};
        Transform::fillMatrixf(*transform, uniformData.terrainTransformMatrix);
        uniformData.terrainScale.x = (float)quadtreeTerrain.getSize().x;
        uniformData.terrainScale.y = (float)quadtreeTerrain.getSize().y;
        uniformData.terrainScale.z = (float)quadtreeTerrain.getHeightScale();
        uniformData.heightmapTextureIndex = UINT32_MAX;
        uniformData.tileGridSize = tileGridSize;//quadtreeTerrain.getTileGridSize();

        std::shared_ptr<TerrainTileSupplier> tileSupplier = quadtreeTerrain.getTileSupplier();
        if (tileSupplier != nullptr) {
            uniformData.heightmapTextureIndex = (uint32_t)m_heightmapImageViews.size();
            const std::vector<ImageView*>& loadedTileImageViews = tileSupplier->getLoadedTileImageViews();
            m_heightmapImageViews.insert(m_heightmapImageViews.end(), loadedTileImageViews.begin(), loadedTileImageViews.end());
        }

        m_terrainUniformData.emplace_back(uniformData);
    }

    if (!m_heightmapImageViews.empty()) {
        uint32_t maxArrayCount = m_terrainDescriptorSetLayout->getBinding(TERRAIN_HEIGHTMAP_TEXTURES_BINDING).descriptorCount;
        DescriptorSetWriter(m_resources->terrainDescriptorSet)
                .writeImage(TERRAIN_HEIGHTMAP_TEXTURES_BINDING, m_defaultHeightmapSampler.get(), m_heightmapImageViews.data(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, glm::min((uint32_t)m_heightmapImageViews.size(), maxArrayCount))
                .write();
    }
}

void TerrainRenderer::applyVisibility() {
    assert(m_visibilityApplied == false);

    if (!m_globalTerrainInstances.empty()) {
        if (!m_terrainUniformData.empty()) {
            GPUTerrainUniformData* mappedTerrainUniformBuffer = static_cast<GPUTerrainUniformData*>(mapTerrainUniformBuffer(m_terrainUniformData.size()));
            memcpy(&mappedTerrainUniformBuffer[0], &m_terrainUniformData[0], m_terrainUniformData.size() * sizeof(GPUTerrainUniformData));
        }

        if (!m_terrainTileDataBuffer.empty()) {
            GPUTerrainTileData* mappedTerrainTileDataBuffer = static_cast<GPUTerrainTileData*>(mapTerrainTileDataBuffer(m_terrainTileDataBuffer.capacity()));
            memcpy(&mappedTerrainTileDataBuffer[0], &m_terrainTileDataBuffer[0], m_terrainTileDataBuffer.size() * sizeof(GPUTerrainTileData));
        }
    }
    m_visibilityApplied = true;
}

uint32_t TerrainRenderer::updateVisibility(double dt, const RenderCamera* renderCamera, const Frustum* frustum) {
    assert(m_visibilityApplied == false);
    const auto& terrainEntities = m_scene->registry()->view<QuadtreeTerrainComponent>();

    // Presumably there will not normally be a large number of terrain entities. It is best to have one entity with the terrain component containing global terrain settings.
    // Multiple terrain entities may be used in situations where the world is enormous, and chunked up, or in the case of planet rendering, each planet might have its own global terrain component.

    uint32_t visibilityIndex = (uint32_t)m_visibilityIndices.size();
    VisibilityIndices& visibility = m_visibilityIndices.emplace_back();
    visibility.firstInstance = (uint32_t)m_globalTerrainInstances.size();

    Transform identityTransform = Transform();

    for (auto it = terrainEntities.begin(); it != terrainEntities.end(); ++it) {
        Entity entity(m_scene, *it);

        const QuadtreeTerrainComponent& quadtreeTerrain = terrainEntities.get<QuadtreeTerrainComponent>(*it);
        const Transform* transform = entity.tryGetComponent<Transform>();
        if (transform == nullptr)
            transform = &identityTransform;

        InstanceInfo instanceInfo{};
        instanceInfo.firstInstance = (uint32_t)m_terrainTileDataBuffer.size();

        updateQuadtreeTerrainTiles(quadtreeTerrain, *transform, dt, frustum);
        instanceInfo.instanceCount = (uint32_t)(m_terrainTileDataBuffer.size() - instanceInfo.firstInstance);
        m_globalTerrainInstances.emplace_back(instanceInfo);
    }

    visibility.instanceCount = (uint32_t)(m_globalTerrainInstances.size() - visibility.firstInstance);
    return visibilityIndex;
}

void TerrainRenderer::drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex) {
    PROFILE_SCOPE("TerrainRenderer::drawTerrain")
    PROFILE_BEGIN_GPU_CMD("TerrainRenderer::renderGeometryPass", commandBuffer);

    // Presumably there will not normally be a large number of terrain entities. It is best to have one entity with the terrain component containing global terrain settings.
    // Multiple terrain entities may be used in situations where the world is enormous, and chunked up, or in the case of planet rendering, each planet might have its own global terrain component.

    if (m_visibilityIndices.empty()) {
        // There are no viewpoints to render the terrain for.
        return;
    }

    assert(visibilityIndex < m_visibilityIndices.size());

    const VisibilityIndices& visibility = m_visibilityIndices[visibilityIndex];
    if (visibility.instanceCount == 0) {
        // No terrain is visible from this viewpoint.
        return;
    }

    assert(visibility.firstInstance < m_globalTerrainInstances.size() && visibility.firstInstance + visibility.instanceCount <= m_globalTerrainInstances.size());

    std::array<vk::DescriptorSet, 2> descriptorSets = {
            Engine::instance()->getDeferredRenderer()->getGlobalDescriptorSet()->getDescriptorSet(),
            m_resources->terrainDescriptorSet->getDescriptorSet()
    };

    std::array<uint32_t, 1> dynamicOffsets{};

    uint32_t vertexCount = getTileVertexCount(tileGridSize);

    auto graphicsPipeline = Engine::instance()->getDeferredRenderer()->getTerrainGeometryGraphicsPipeline();

    for (uint32_t i = visibility.firstInstance; i < visibility.firstInstance + visibility.instanceCount; ++i) {
        const InstanceInfo& instanceInfo = m_globalTerrainInstances[i];

        vk::DeviceSize alignedUniformBufferSize = Engine::graphics()->getAlignedUniformBufferOffset(sizeof(GPUTerrainUniformData));
        dynamicOffsets[0] = (uint32_t)(alignedUniformBufferSize * i);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);
        commandBuffer.draw(vertexCount, instanceInfo.instanceCount, 0, instanceInfo.firstInstance);
//            m_terrainTileMesh->draw(commandBuffer, instanceInfo.instanceCount, instanceInfo.firstInstance);
    }

    PROFILE_END_GPU_CMD("TerrainRenderer::renderGeometryPass", commandBuffer);
}

void TerrainRenderer::recordRenderCommands(double dt, const vk::CommandBuffer& commandBuffer, uint32_t visibilityIndex) {
    PROFILE_SCOPE("TerrainRenderer::recordRenderCommands")
    PROFILE_BEGIN_GPU_CMD("TerrainRenderer::recordRenderCommands", commandBuffer);

    if (m_visibilityIndices.empty()) {
        // There are no viewpoints to render the terrain for.
        return;
    }

    assert(visibilityIndex < m_visibilityIndices.size());

    const VisibilityIndices& visibility = m_visibilityIndices[visibilityIndex];
    if (visibility.instanceCount == 0) {
        // No terrain is visible from this viewpoint.
        return;
    }

    assert(visibility.firstInstance < m_globalTerrainInstances.size() && visibility.firstInstance + visibility.instanceCount < m_globalTerrainInstances.size());

    uint32_t vertexCount = getTileVertexCount(tileGridSize);

    for (uint32_t i = visibility.firstInstance; i < visibility.firstInstance + visibility.instanceCount; ++i) {
        const InstanceInfo& instanceInfo = m_globalTerrainInstances[i];

        commandBuffer.draw(vertexCount, instanceInfo.instanceCount, 0, instanceInfo.firstInstance);
//        m_terrainTileMesh->draw(commandBuffer, instanceInfo.instanceCount, instanceInfo.firstInstance);
    }

    PROFILE_END_GPU_CMD("TerrainRenderer::recordRenderCommands", commandBuffer);

}

void TerrainRenderer::updateQuadtreeTerrainTiles(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const Frustum* frustum) {
    PROFILE_SCOPE("TerrainRenderer::updateQuadtreeTerrainTiles")

    quadtreeTerrain.getTileQuadtree()->setTransform(transform);
    quadtreeTerrain.getTileQuadtree()->update(frustum);

    glm::dvec3 terrainScale(quadtreeTerrain.getSize().x, quadtreeTerrain.getHeightScale(), quadtreeTerrain.getSize().y);

    std::vector<TerrainTileQuadtree::TraversalInfo> traversalStack;

    quadtreeTerrain.getTileQuadtree()->traverseTreeNodes(traversalStack, [this](TerrainTileQuadtree* tileQuadtree, const TerrainTileQuadtree::TraversalInfo& traversalInfo) {
        if (!tileQuadtree->isVisible(traversalInfo.nodeIndex)) {
            return true; // Skip whole subtree
        }
        if (tileQuadtree->hasChildren(traversalInfo.nodeIndex)) {
            return false; // Continue down the tree.
        }

        glm::dvec2 treePosition = glm::dvec2(traversalInfo.treePosition);// + glm::dvec2(0.5);
        glm::dvec2 tileCoord = tileQuadtree->getNormalizedNodeCoordinate(treePosition, traversalInfo.treeDepth);
        double tileSize = tileQuadtree->getNormalizedNodeSizeForTreeDepth(traversalInfo.treeDepth);

//        double x = tileCoord.x * terrainScale.x - terrainScale.x * 0.5;
//        double y = 0.0F;// - traversalInfo.treeDepth * 0.1;
//        double z = tileCoord.y * terrainScale.z - terrainScale.z * 0.5;
//        Transform tileTransform;
//        tileTransform.translate(x, y, z);
//        tileTransform.scale(tileSize * terrainScale);

        GPUTerrainTileData& terrainData = m_terrainTileDataBuffer.emplace_back();
        terrainData.tilePosition = (tileCoord - 0.5) * tileQuadtree->getSize();
        terrainData.tileSize = glm::vec2(tileSize * tileQuadtree->getSize());
        terrainData.textureOffset = glm::vec2(tileCoord);
        terrainData.textureSize = glm::vec2((float)tileSize);

        return false;
    });
}

void TerrainRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* TerrainRenderer::getScene() const {
    return m_scene;
}

const SharedResource<DescriptorSetLayout>& TerrainRenderer::getTerrainDescriptorSetLayout() const {
    return m_terrainDescriptorSetLayout;
}

DescriptorSet* TerrainRenderer::getTerrainDescriptorSet() const {
    return m_resources->terrainDescriptorSet;
}

uint32_t TerrainRenderer::getTileVertexCount(uint32_t tileSize) {
    uint32_t verticesPerStrip = 2 + (tileGridSize * 2) + 1;
    uint32_t vertexCount = (verticesPerStrip + 1) * tileGridSize;
    return vertexCount;
}

const std::vector<TerrainRenderer::InstanceInfo>& TerrainRenderer::getGlobalTerrainInstances() const {
    return m_globalTerrainInstances;
}

void* TerrainRenderer::mapTerrainTileDataBuffer(size_t maxObjects) {

    vk::DeviceSize newBufferSize = sizeof(GPUTerrainTileData) * maxObjects;

    if (m_resources->terrainTileDataBuffer == nullptr || newBufferSize > m_resources->terrainTileDataBuffer->getSize()) {

        BufferConfiguration terrainTileDataBufferConfig{};
        terrainTileDataBufferConfig.device = Engine::graphics()->getDevice();
        terrainTileDataBufferConfig.size = newBufferSize;
        terrainTileDataBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        terrainTileDataBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        delete m_resources->terrainTileDataBuffer;
        m_resources->terrainTileDataBuffer = Buffer::create(terrainTileDataBufferConfig, "SceneRenderer-TerrainTileDataBuffer");

        DescriptorSetWriter(m_resources->terrainDescriptorSet)
                .writeBuffer(TERRAIN_TILE_DATA_BUFFER_BINDING, m_resources->terrainTileDataBuffer, 0, newBufferSize)
                .write();
    }

    void* mappedBuffer = m_resources->terrainTileDataBuffer->map();
    return mappedBuffer;
}


void* TerrainRenderer::mapTerrainUniformBuffer(size_t maxObjects) {
    vk::DeviceSize alignedSize = Engine::graphics()->getAlignedUniformBufferOffset(sizeof(GPUTerrainUniformData));
    vk::DeviceSize newBufferSize = alignedSize * maxObjects;

    if (m_resources->terrainUniformBuffer == nullptr || newBufferSize > m_resources->terrainUniformBuffer->getSize()) {

        BufferConfiguration terrainUniformBufferConfig{};
        terrainUniformBufferConfig.device = Engine::graphics()->getDevice();
        terrainUniformBufferConfig.size = newBufferSize;
        terrainUniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        terrainUniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        delete m_resources->terrainUniformBuffer;
        m_resources->terrainUniformBuffer = Buffer::create(terrainUniformBufferConfig, "SceneRenderer-TerrainUniformBuffer");

        DescriptorSetWriter(m_resources->terrainDescriptorSet)
                .writeBuffer(TERRAIN_UNIFORM_BUFFER_BINDING, m_resources->terrainUniformBuffer, 0, alignedSize)
                .write();
    }

    void* mappedBuffer = m_resources->terrainUniformBuffer->map();
    return mappedBuffer;
}

void TerrainRenderer::initializeDefaultEmptyHeightmapTexture() {
    union {
        float value;
        uint8_t valueBytes[4];
    };
    value = 0.0F;

    ImageData imageData(valueBytes, 1, 1, ImagePixelLayout::R, ImagePixelFormat::Float32);

    Image2DConfiguration defaultEmptyHeightmapImageConfig{};
    defaultEmptyHeightmapImageConfig.device = Engine::graphics()->getDevice();
    defaultEmptyHeightmapImageConfig.format = vk::Format::eR32Sfloat;
    defaultEmptyHeightmapImageConfig.imageData = &imageData;
    m_defaultEmptyHeightmapImage = std::shared_ptr<Image2D>(Image2D::create(defaultEmptyHeightmapImageConfig, "TerrainRenderer-DefaultEmptyHeightmapImage"));

    ImageViewConfiguration defaultEmptyHeightmapImageViewConfig{};
    defaultEmptyHeightmapImageViewConfig.device = Engine::graphics()->getDevice();
    defaultEmptyHeightmapImageViewConfig.format = vk::Format::eR32Sfloat;
    defaultEmptyHeightmapImageViewConfig.setSwizzle(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eR);
    defaultEmptyHeightmapImageViewConfig.setImage(m_defaultEmptyHeightmapImage.get());
    m_defaultEmptyHeightmapImageView = std::shared_ptr<ImageView>(ImageView::create(defaultEmptyHeightmapImageViewConfig, "TerrainRenderer-DefaultEmptyHeightmapImageView"));

    SamplerConfiguration defaultEmptyHeightmapSamplerConfig{};
    defaultEmptyHeightmapSamplerConfig.device = Engine::graphics()->getDevice();
    defaultEmptyHeightmapSamplerConfig.minFilter = vk::Filter::eLinear;
    defaultEmptyHeightmapSamplerConfig.magFilter = vk::Filter::eLinear;
    m_defaultHeightmapSampler = std::shared_ptr<Sampler>(Sampler::create(defaultEmptyHeightmapSamplerConfig, "TerrainRenderer-DefaultEmptyHeightmapSampler"));
}