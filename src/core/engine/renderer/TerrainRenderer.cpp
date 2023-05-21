
#include "TerrainRenderer.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/terrain/QuadtreeTerrain.h"
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
}

bool TerrainRenderer::init() {
    LOG_INFO("Initializing TerrainRenderer");

    MeshData<Vertex> terrainTileMeshData;
    terrainTileMeshData.createPlane(glm::vec3(-1.0F, -1.0F, -1.0F), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec2(2.0F), glm::uvec2(15, 15));

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

    for (auto it = terrainEntities.begin(); it != terrainEntities.end(); ++it) {
        const QuadtreeTerrainComponent& quadtreeTerrain = terrainEntities.get<QuadtreeTerrainComponent>(*it);
        const Transform& transform = Transform(); // TODO: get entity transform if it exists, else use no transform
        drawQuadtreeTerrain(quadtreeTerrain, transform, dt, commandBuffer, frustum);
    }
}

void TerrainRenderer::drawQuadtreeTerrain(const QuadtreeTerrainComponent& quadtreeTerrain, const Transform& transform, double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum) {
    m_terrainTileMesh->draw(commandBuffer, 1, 0);
}

void TerrainRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* TerrainRenderer::getScene() const {
    return m_scene;
}