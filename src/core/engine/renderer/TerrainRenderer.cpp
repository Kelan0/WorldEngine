
#include "TerrainRenderer.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"
#include "core/engine/renderer/Material.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/util/Logger.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "RenderComponent.h"

#define TERRAIN_UNIFORM_BUFFER_BINDING 0
#define TERRAIN_TILE_DATA_BUFFER_BINDING 1

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

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    m_terrainDescriptorSetLayout = builder
            .addUniformBuffer(TERRAIN_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eVertex, true) // global terrain uniform buffer
            .addStorageBuffer(TERRAIN_TILE_DATA_BUFFER_BINDING, vk::ShaderStageFlagBits::eVertex, false) // terrain tile data buffer
            .build("TerrainRenderer-TerrainDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->terrainDescriptorSet = DescriptorSet::create(m_terrainDescriptorSetLayout, descriptorPool, "TerrainRenderer-TerrainDescriptorSetLayout");
        m_resources[i]->terrainTileDataBuffer = nullptr;
        m_resources[i]->terrainUniformBuffer = nullptr;
    }

    MeshData<Vertex> terrainTileMeshData;
    terrainTileMeshData.createPlane(glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec2(1.0F), glm::uvec2(15, 15));

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
}

void TerrainRenderer::renderGeometryPass(double dt, const vk::CommandBuffer& commandBuffer, const RenderCamera* renderCamera, const Frustum* frustum) {
    PROFILE_SCOPE("TerrainRenderer::renderGeometryPass");
    PROFILE_BEGIN_GPU_CMD("TerrainRenderer::renderGeometryPass", commandBuffer);

    drawTerrain(dt, commandBuffer, frustum);

    PROFILE_END_GPU_CMD("TerrainRenderer::renderGeometryPass", commandBuffer);
}

void TerrainRenderer::drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum) {
    PROFILE_SCOPE("TerrainRenderer::drawTerrain")

    const auto& terrainEntities = m_scene->registry()->view<QuadtreeTerrainComponent>();

    // Presumably there will not normally be a large number of terrain entities. It is best to have one entity with the terrain component containing global terrain settings.
    // Multiple terrain entities may be used in situations where the world is enormous, and chunked up, or in the case of planet rendering, each planet might have its own global terrain component.

    m_terrainTileDataBuffer.clear();

    struct InstanceInfo {
        uint32_t firstInstance;
        uint32_t instanceCount;
    };

    std::vector<InstanceInfo> terrainInstances;
    std::vector<GPUTerrainUniformData> terrainUniformData;

    Transform identityTransform = Transform();

    size_t numTerrains = 0;

    for (auto it = terrainEntities.begin(); it != terrainEntities.end(); ++it, ++numTerrains) {
        Entity entity(m_scene, *it);

        const QuadtreeTerrainComponent& quadtreeTerrain = terrainEntities.get<QuadtreeTerrainComponent>(*it);
        const Transform* transform = entity.tryGetComponent<Transform>();
        if (transform == nullptr)
            transform = &identityTransform;

        GPUTerrainUniformData uniformData{};
        Transform::fillMatrixf(*transform, uniformData.terrainTransformMatrix);

        InstanceInfo instanceInfo{};
        instanceInfo.firstInstance = (uint32_t)m_terrainTileDataBuffer.size();

        updateQuadtreeTerrainTiles(quadtreeTerrain, *transform, dt, frustum);
        instanceInfo.instanceCount = (uint32_t)(m_terrainTileDataBuffer.size() - instanceInfo.firstInstance);

        if (instanceInfo.instanceCount > 0) {
            terrainInstances.emplace_back(instanceInfo);
            terrainUniformData.emplace_back(uniformData);
        }
    }

    if (!terrainInstances.empty()) {
        GPUTerrainUniformData* mappedTerrainUniformBuffer = static_cast<GPUTerrainUniformData*>(mapTerrainUniformBuffer(numTerrains));
        memcpy(&mappedTerrainUniformBuffer[0], &terrainUniformData[0], terrainUniformData.size() * sizeof(GPUTerrainUniformData));

        GPUTerrainTileData* mappedTerrainTileDataBuffer = static_cast<GPUTerrainTileData*>(mapTerrainTileDataBuffer(m_terrainTileDataBuffer.capacity()));
        memcpy(&mappedTerrainTileDataBuffer[0], &m_terrainTileDataBuffer[0], m_terrainTileDataBuffer.size() * sizeof(GPUTerrainTileData));

        auto graphicsPipeline = Engine::instance()->getDeferredRenderer()->getTerrainGeometryGraphicsPipeline();

        std::array<vk::DescriptorSet, 2> descriptorSets = {
                Engine::instance()->getDeferredRenderer()->getGlobalDescriptorSet()->getDescriptorSet(),
                m_resources->terrainDescriptorSet->getDescriptorSet()
        };

        std::array<uint32_t, 1> dynamicOffsets{};

        graphicsPipeline->bind(commandBuffer);

        for (int i = 0; i < terrainInstances.size(); ++i) {
            const InstanceInfo& instanceInfo = terrainInstances[i];

            dynamicOffsets[0] = sizeof(GPUTerrainUniformData) * i;

            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);
            m_terrainTileMesh->draw(commandBuffer, instanceInfo.instanceCount, instanceInfo.firstInstance);
        }
    }
}

void TerrainRenderer::updateQuadtreeTerrainTiles(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const Frustum* frustum) {
    PROFILE_SCOPE("TerrainRenderer::updateQuadtreeTerrainTiles")

    quadtreeTerrain.getTileQuadtree()->setTransform(transform);
    quadtreeTerrain.getTileQuadtree()->update(frustum);

    glm::dvec3 terrainScale(quadtreeTerrain.getSize().x, quadtreeTerrain.getHeightScale(), quadtreeTerrain.getSize().y);


    quadtreeTerrain.getTileQuadtree()->forEachLeafNode([this, &terrainScale](TerrainTileQuadtree* tileQuadtree, const TerrainTileQuadtree::TileTreeNode& node, size_t index) {

        if (!TerrainTileQuadtree::isVisible(node))
            return; // Ignore invisible nodes

        glm::dvec2 treePosition = glm::dvec2(node.treePosition);// + glm::dvec2(0.5);
        glm::dvec2 coord = tileQuadtree->getNormalizedNodeCoordinate(treePosition, node.treeDepth);
        double size = tileQuadtree->getNormalizedNodeSizeForTreeDepth(node.treeDepth);

        double x = coord.x * terrainScale.x - terrainScale.x * 0.5;
        double y = 0.0F;// - node.treeDepth * 0.1;
        double z = coord.y * terrainScale.z - terrainScale.z * 0.5;
        Transform tileTransform;
        tileTransform.translate(x, y, z);
        tileTransform.scale(size * terrainScale);

        GPUTerrainTileData& terrainData = m_terrainTileDataBuffer.emplace_back();
        Transform::fillMatrixf(tileTransform, terrainData.modelMatrix);
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

    vk::DeviceSize newBufferSize = sizeof(GPUTerrainUniformData) * maxObjects;

    if (m_resources->terrainUniformBuffer == nullptr || newBufferSize > m_resources->terrainUniformBuffer->getSize()) {

        BufferConfiguration terrainUniformBufferConfig{};
        terrainUniformBufferConfig.device = Engine::graphics()->getDevice();
        terrainUniformBufferConfig.size = newBufferSize;
        terrainUniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        terrainUniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        delete m_resources->terrainUniformBuffer;
        m_resources->terrainUniformBuffer = Buffer::create(terrainUniformBufferConfig, "SceneRenderer-TerrainUniformBuffer");

        DescriptorSetWriter(m_resources->terrainDescriptorSet)
                .writeBuffer(TERRAIN_UNIFORM_BUFFER_BINDING, m_resources->terrainUniformBuffer, 0, sizeof(GPUTerrainUniformData))
                .write();
    }

    void* mappedBuffer = m_resources->terrainUniformBuffer->map();
    return mappedBuffer;
}