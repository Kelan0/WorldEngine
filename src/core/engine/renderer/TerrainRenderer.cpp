
#include "TerrainRenderer.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"
#include "core/engine/renderer/Material.h"
#include "core/graphics/Mesh.h"
#include "core/util/Logger.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "RenderComponent.h"





TerrainRenderer::TerrainRenderer():
    m_scene(nullptr),
    m_terrainTileMesh(nullptr) {
}

TerrainRenderer::~TerrainRenderer() {
    LOG_INFO("Destroying TerrainRenderer");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        if (m_resources[i] != nullptr) {
            delete m_resources[i]->terrainDataBuffer;
            delete m_resources[i]->terrainDescriptorSet;
        }
    }
}

bool TerrainRenderer::init() {
    LOG_INFO("Initializing TerrainRenderer");

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    m_terrainDescriptorSetLayout = builder
            .addStorageBuffer(0, vk::ShaderStageFlagBits::eVertex) // terrain data buffer (transform and other info)
            .build("TerrainRenderer-TerrainDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->terrainDescriptorSet = DescriptorSet::create(m_terrainDescriptorSetLayout, descriptorPool, "TerrainRenderer-TerrainDescriptorSetLayout");
        m_resources[i]->terrainDataBuffer = nullptr;
    }

    MeshData<Vertex> terrainTileMeshData;
    terrainTileMeshData.createPlane(glm::vec3(-0.5F, 0.0F, -0.5F), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec2(1.0F), glm::uvec2(15, 15));

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

}

void TerrainRenderer::drawTerrain(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum) {
    const auto& terrainEntities = m_scene->registry()->view<QuadtreeTerrainComponent>();

    // Presumably there will not normally be a large number of terrain entities. It is best to have one entity with the terrain component containing global terrain settings.
    // Multiple terrain entities may be used in situations where the world is enormous, and chunked up, or in the case of planet rendering, each planet might have its own global terrain component.

    m_terrainDataBuffer.clear();

    for (auto it = terrainEntities.begin(); it != terrainEntities.end(); ++it) {
        const QuadtreeTerrainComponent& quadtreeTerrain = terrainEntities.get<QuadtreeTerrainComponent>(*it);
        const Transform& transform = Transform(); // TODO: get entity transform if it exists, else use no transform
        updateQuadtreeTerrainTiles(quadtreeTerrain, transform, dt, frustum);
    }

    if (!m_terrainDataBuffer.empty()) {
        GPUTerrainData *mappedTerrainDataBuffer = static_cast<GPUTerrainData *>(mapTerrainDataBuffer(m_terrainDataBuffer.size()));
        memcpy(&mappedTerrainDataBuffer[0], &m_terrainDataBuffer[0], m_terrainDataBuffer.size() * sizeof(GPUTerrainData));

        m_terrainTileMesh->draw(commandBuffer, m_terrainDataBuffer.size(), 0);
    }
}

void TerrainRenderer::updateQuadtreeTerrainTiles(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const Frustum* frustum) {
    quadtreeTerrain.getTileQuadtree()->setTransform(transform);
    quadtreeTerrain.getTileQuadtree()->update(frustum);

    const std::vector<TerrainTileQuadtree::NodeQuad>& nodes = quadtreeTerrain.getTileQuadtree()->getLeafNodeQuads();

    Transform tileTransform{};

    glm::dvec3 terrainScale(quadtreeTerrain.getSize().x, quadtreeTerrain.getHeightScale(), quadtreeTerrain.getSize().y);

    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        const TerrainTileQuadtree::NodeQuad& nodeQuad = *it;

        tileTransform.setTranslation(nodeQuad.normalizedCoord.x * terrainScale.x, 0.0F, nodeQuad.normalizedCoord.y * terrainScale.z);
        tileTransform.setScale(nodeQuad.normalizedSize * terrainScale);

        GPUTerrainData& terrainData = m_terrainDataBuffer.emplace_back();
        Transform::fillMatrixf(tileTransform, terrainData.modelMatrix);
    }

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

void* TerrainRenderer::mapTerrainDataBuffer(size_t maxObjects) {

    vk::DeviceSize newBufferSize = sizeof(GPUTerrainData) * maxObjects;

    if (m_resources->terrainDataBuffer == nullptr || newBufferSize > m_resources->terrainDataBuffer->getSize()) {

        BufferConfiguration terrainDataBufferConfig{};
        terrainDataBufferConfig.device = Engine::graphics()->getDevice();
        terrainDataBufferConfig.size = newBufferSize;
        terrainDataBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        terrainDataBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        delete m_resources->terrainDataBuffer;
        m_resources->terrainDataBuffer = Buffer::create(terrainDataBufferConfig, "SceneRenderer-TerrainDataBuffer");

        DescriptorSetWriter(m_resources->terrainDescriptorSet)
                .writeBuffer(0, m_resources->terrainDataBuffer, 0, newBufferSize)
                .write();
    }

    void* mappedBuffer = m_resources->terrainDataBuffer->map();
    return mappedBuffer;
}