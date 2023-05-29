#include "core/engine/renderer/SceneRenderer.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/engine/renderer/Material.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/util/Logger.h"


template<typename T>
struct StaticUpdateFlag { uint8_t _v; };

template<typename T>
struct DynamicUpdateFlag { uint8_t _v; };

template<typename T>
struct AlwaysUpdateFlag { uint8_t _v; };

SceneRenderer::SceneRenderer():
    m_scene(nullptr),
    m_numRenderEntities(0),
    m_previousPartialTicks(1.0),
    m_numAddedRenderEntities(0) {
}

SceneRenderer::~SceneRenderer() {
    LOG_INFO("Destroying SceneRenderer");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        if (m_resources[i] != nullptr) {
            delete m_resources[i]->objectIndicesBuffer;
            delete m_resources[i]->worldTransformBuffer;
            delete m_resources[i]->materialDataBuffer;
            delete m_resources[i]->objectDescriptorSet;
            delete m_resources[i]->materialDescriptorSet;
        }
    }
}

bool SceneRenderer::init() {
    LOG_INFO("Initializing SceneRenderer");

    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    constexpr size_t maxTextures = 0xFFFF;

    initMissingTextureMaterial();
    std::vector<Texture*> initialTextures;
    initialTextures.resize(maxTextures, m_missingTextureMaterial->getAlbedoMap().get());

    std::vector<vk::ImageLayout> initialImageLayouts;
    initialImageLayouts.resize(maxTextures, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_objectDescriptorSetLayout = builder
            .addStorageBuffer(0, vk::ShaderStageFlagBits::eVertex) // object data buffer (transform and other info)
            .addStorageBuffer(1, vk::ShaderStageFlagBits::eVertex) // object indices buffer
            .build("SceneRenderer-ObjectDescriptorSetLayout");

    m_materialDescriptorSetLayout = builder
            .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, maxTextures) // All material textures
            .addStorageBuffer(1, vk::ShaderStageFlagBits::eFragment) // material data buffer (material properties & texture indices)
            .build("SceneRenderer-MaterialDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->objectDescriptorSet = DescriptorSet::create(m_objectDescriptorSetLayout, descriptorPool, "SceneRenderer-ObjectDescriptorSet");
        m_resources[i]->materialDescriptorSet = DescriptorSet::create(m_materialDescriptorSetLayout, descriptorPool, "SceneRenderer-MaterialDescriptorSet");

        m_resources[i]->objectIndicesBuffer = nullptr;
        m_resources[i]->worldTransformBuffer = nullptr;
        m_resources[i]->materialDataBuffer = nullptr;

        m_resources[i]->updateTextureDescriptorStartIndex = UINT32_MAX;
        m_resources[i]->updateTextureDescriptorEndIndex = 0;

        DescriptorSetWriter(m_resources[i]->materialDescriptorSet)
                .writeImage(0, initialTextures.data(), initialImageLayouts.data(), 0, maxTextures)
                .write();
    }

    // Missing texture needs to be at index 0
    m_materialIndices.clear();
    m_materialDataBuffer.clear();
    registerMaterial(m_missingTextureMaterial.get());

    m_scene->enableEvents<RenderComponent>();
    m_scene->getEventDispatcher()->connect<ComponentAddedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentAdded, this);
    m_scene->getEventDispatcher()->connect<ComponentRemovedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentRemoved, this);

    m_previousPartialTicks = 1.0; // First frame considers one tick to have passed (m_previousPartialTicks > current partialTicks)

    return true;
}

void SceneRenderer::preRender(double dt) {
    PROFILE_SCOPE("SceneRenderer::preRender")

    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();
    m_numRenderEntities = (uint32_t)renderEntities.size();

    sortRenderEntities();
    updateEntityWorldTransforms();
    updateEntityMaterials();
    streamEntityRenderData();

    m_previousPartialTicks = Engine::instance()->getPartialTicks();
    m_numAddedRenderEntities = 0;
}

void SceneRenderer::renderGeometryPass(double dt, const vk::CommandBuffer& commandBuffer, const RenderCamera* renderCamera, const Frustum* frustum) {
    PROFILE_SCOPE("SceneRenderer::renderGeometryPass");
    PROFILE_BEGIN_GPU_CMD("SceneRenderer::renderGeometryPass", commandBuffer);

    auto graphicsPipeline = Engine::instance()->getDeferredRenderer()->getEntityGeometryGraphicsPipeline();

    std::array<vk::DescriptorSet, 3> descriptorSets = {
            Engine::instance()->getDeferredRenderer()->getGlobalDescriptorSet()->getDescriptorSet(),
            Engine::instance()->getSceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
            Engine::instance()->getSceneRenderer()->getMaterialDescriptorSet()->getDescriptorSet(),
    };

    std::array<uint32_t, 0> dynamicOffsets = {}; // zero-size array okay?

    graphicsPipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

    Engine::instance()->getSceneRenderer()->drawEntities(dt, commandBuffer, frustum);
    PROFILE_END_GPU_CMD("SceneRenderer::renderGeometryPass", commandBuffer);
}

void SceneRenderer::drawEntities(double dt, const vk::CommandBuffer& commandBuffer, const Frustum* frustum) {
    PROFILE_SCOPE("SceneRenderer::drawEntities")
    PROFILE_BEGIN_GPU_CMD("SceneRenderer::drawEntities", commandBuffer);

    applyFrustumCulling(frustum);
    recordRenderCommands(dt, commandBuffer);

    PROFILE_END_GPU_CMD("SceneRenderer::drawEntities", commandBuffer);
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}

const SharedResource<DescriptorSetLayout>& SceneRenderer::getObjectDescriptorSetLayout() const {
    return m_objectDescriptorSetLayout;
}

const SharedResource<DescriptorSetLayout>& SceneRenderer::getMaterialDescriptorSetLayout() const {
    return m_materialDescriptorSetLayout;
}

DescriptorSet* SceneRenderer::getObjectDescriptorSet() const {
    return m_resources->objectDescriptorSet;
}

DescriptorSet* SceneRenderer::getMaterialDescriptorSet() const {
    return m_resources->materialDescriptorSet;
}

uint32_t SceneRenderer::registerTexture(Texture* texture) {
    assert(texture != nullptr);

    uint32_t textureIndex;
    auto it = m_textureIndices.find(texture->getResourceId());
    if (it == m_textureIndices.end()) {
        textureIndex = (uint32_t)m_textures.size();
        m_textures.emplace_back(texture);

        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
            m_resources[i]->updateTextureDescriptorStartIndex = glm::min(m_resources[i]->updateTextureDescriptorStartIndex, textureIndex);
            m_resources[i]->updateTextureDescriptorEndIndex = glm::max(m_resources[i]->updateTextureDescriptorEndIndex, textureIndex);
        }

        m_textureIndices.insert(std::make_pair(texture->getResourceId(), textureIndex));
    } else {
        textureIndex = it->second;
    }

    return textureIndex;
}

uint32_t SceneRenderer::registerMaterial(Material* material) {
    assert(material != nullptr);
    uint32_t materialIndex;

    auto it = m_materialIndices.find(material->getResourceId());
    if (it == m_materialIndices.end()) {
        materialIndex = (uint32_t)m_materialDataBuffer.size();

        GPUMaterial gpuMaterial{};
        gpuMaterial.hasAlbedoTexture = material->hasAlbedoMap();
        gpuMaterial.hasRoughnessTexture = material->hasRoughnessMap();
        gpuMaterial.hasMetallicTexture = material->hasMetallicMap();
        gpuMaterial.hasEmissionTexture = material->hasEmissionMap();
        gpuMaterial.hasNormalTexture = material->hasNormalMap();
        if (gpuMaterial.hasAlbedoTexture) gpuMaterial.albedoTextureIndex = registerTexture(material->getAlbedoMap().get());
        if (gpuMaterial.hasRoughnessTexture) gpuMaterial.roughnessTextureIndex = registerTexture(material->getRoughnessMap().get());
        if (gpuMaterial.hasMetallicTexture) gpuMaterial.metallicTextureIndex = registerTexture(material->getMetallicMap().get());
        if (gpuMaterial.hasEmissionTexture) gpuMaterial.emissionTextureIndex = registerTexture(material->getEmissionMap().get());
        if (gpuMaterial.hasNormalTexture) gpuMaterial.normalTextureIndex = registerTexture(material->getNormalMap().get());
        gpuMaterial.albedoColour_r = material->getAlbedo().r;
        gpuMaterial.albedoColour_g = material->getAlbedo().g;
        gpuMaterial.albedoColour_b = material->getAlbedo().b;
        gpuMaterial.emission_r = material->getEmission().r;
        gpuMaterial.emission_g = material->getEmission().g;
        gpuMaterial.emission_b = material->getEmission().b;
        gpuMaterial.roughness = material->getRoughness();
        gpuMaterial.metallic = material->getMetallic();
        m_materialDataBuffer.emplace_back(gpuMaterial);

        m_materialIndices.insert(std::make_pair(material->getResourceId(), materialIndex));
    } else {
        materialIndex = it->second;
    }
    return materialIndex;
}

void SceneRenderer::initMissingTextureMaterial() {
    union {
        glm::u8vec4 pixels[2][2];
        uint8_t pixelBytes[sizeof(glm::u8vec4) * 4];
    };

    pixels[0][0] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);
    pixels[0][1] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][0] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][1] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);

    ImageData imageData(pixelBytes, 2, 2, ImagePixelLayout::RGBA, ImagePixelFormat::UInt8);

    Image2DConfiguration missingTextureImageConfig{};
    missingTextureImageConfig.device = Engine::graphics()->getDevice();
    missingTextureImageConfig.format = vk::Format::eR8G8B8A8Unorm;
    missingTextureImageConfig.imageData = &imageData;
    m_missingTextureImage = std::shared_ptr<Image2D>(Image2D::create(missingTextureImageConfig, "SceneRenderer-MissingTextureImage"));

    ImageViewConfiguration missingTextureImageViewConfig{};
    missingTextureImageViewConfig.device = missingTextureImageConfig.device;
    missingTextureImageViewConfig.format = missingTextureImageConfig.format;
    missingTextureImageViewConfig.setImage(m_missingTextureImage.get());

    SamplerConfiguration missingTextureSamplerConfig{};
    missingTextureSamplerConfig.device = missingTextureImageConfig.device;
    missingTextureSamplerConfig.minFilter = vk::Filter::eNearest;
    missingTextureSamplerConfig.magFilter = vk::Filter::eNearest;

    MaterialConfiguration materialConfig{};
    materialConfig.device = Engine::graphics()->getDevice();
    materialConfig.setAlbedoMap(missingTextureImageViewConfig, missingTextureSamplerConfig, "SceneRenderer-MissingTexture");

    m_missingTextureMaterial = std::shared_ptr<Material>(Material::create(materialConfig, "SceneRenderer-MissingTextureMaterial"));
}

void SceneRenderer::onRenderComponentAdded(ComponentAddedEvent<RenderComponent>* event) {
    assert(event->entity.hasComponent<Transform>());

    RenderInfo& renderInfo = event->entity.addComponent<RenderInfo>();
    WorldRenderBounds& renderBounds = event->entity.addComponent<WorldRenderBounds>();
    Transform& transform = event->entity.getComponent<Transform>();

    renderInfo.materialId = 0;
    renderInfo.materialIndex = UINT32_MAX;
    renderInfo.objectIndex = (uint32_t)m_objectDataBuffer.size();

    GPUObjectData objectData{};
    Transform::fillMatrixf(transform, objectData.modelMatrix);
    m_objectDataBuffer.emplace_back(objectData);

    ++m_numAddedRenderEntities;
}

void SceneRenderer::onRenderComponentRemoved(ComponentRemovedEvent<RenderComponent>* event) {
    RenderInfo& renderInfo = event->entity.getComponent<RenderInfo>();
    if (renderInfo.objectIndex < m_objectDataBuffer.size()) {
        // TODO: add index to removedObjectIndices, then handle this in the next sortRenderEntities call.
    }

    event->entity.removeComponent<RenderInfo>();
    event->entity.removeComponent<WorldRenderBounds>();
}

void SceneRenderer::recordRenderCommands(double dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("SceneRenderer::recordRenderCommands");

    if (m_objectIndicesBuffer.empty()) {
        return; // Nothing to draw ...??
    }
    PROFILE_REGION("Gather DrawCommands")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    m_drawCommands.clear();

    DrawCommand currDrawCommand{};
    currDrawCommand.firstInstance = 0;

    for (uint32_t i = 0; i < m_objectIndicesBuffer.size(); ++i) {
        auto it = renderEntities.begin() + m_objectIndicesBuffer[i];
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);

        if (currDrawCommand.mesh == nullptr) {
            currDrawCommand.mesh = renderComponent.getMesh().get();
            currDrawCommand.instanceCount = 0;

        } else if (currDrawCommand.mesh != renderComponent.getMesh().get()) {
            m_drawCommands.emplace_back(currDrawCommand);
            currDrawCommand.mesh = renderComponent.getMesh().get();
            currDrawCommand.firstInstance += currDrawCommand.instanceCount;
            currDrawCommand.instanceCount = 0;
        }

        ++currDrawCommand.instanceCount;
    }

    if (currDrawCommand.mesh != nullptr && currDrawCommand.instanceCount > 0)
        m_drawCommands.emplace_back(currDrawCommand);

    PROFILE_REGION("Draw meshes")
    for (auto& command : m_drawCommands)
        command.mesh->draw(commandBuffer, command.instanceCount, command.firstInstance);

    PROFILE_END_REGION()
}

void SceneRenderer::applyFrustumCulling(const Frustum* frustum) {
    PROFILE_SCOPE("SceneRenderer::applyFrustumCulling");

    m_objectIndicesBuffer.clear();

    PROFILE_REGION("Update visible indices")
    constexpr bool frustumCullingEnabled = false;

    if (!frustumCullingEnabled || frustum == nullptr) {
        // No frustum, we draw everything
        for (uint32_t i = 0; i < m_numRenderEntities; ++i) {
            m_objectIndicesBuffer.emplace_back(i);
        }

    } else {

        const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

        uint32_t objectIndex = 0;
        for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++objectIndex) {
            const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);
            const Transform& transform = renderEntities.get<Transform>(*it);
//            if (renderComponent.getBoundingVolume() != nullptr) {
//                if (frustum->contains(*renderComponent.getBoundingVolume())) {
//                    m_objectIndicesBuffer.emplace_back(objectIndex);
//                }
//            } else {
                BoundingSphere boundingSphere(transform.getTranslation(), 1.0);
                if (frustum->intersects(boundingSphere)) {
                    m_objectIndicesBuffer.emplace_back(objectIndex);
                }
//            }
        }
    }

    PROFILE_REGION("Upload visible indices")

    if (!m_objectIndicesBuffer.empty()) {
        uint32_t* mappedObjectIndicesBuffer = static_cast<uint32_t*>(mapObjectIndicesBuffer(m_numRenderEntities));
        memcpy(&mappedObjectIndicesBuffer[0], &m_objectIndicesBuffer[0], m_objectIndicesBuffer.size() * sizeof(uint32_t));
    }
}

void SceneRenderer::sortRenderEntities() {
    PROFILE_SCOPE("SceneRenderer::sortRenderEntities")

    auto renderComponentComp = [](const RenderComponent& lhs, const RenderComponent& rhs) {
        if (lhs.transformUpdateType() != rhs.transformUpdateType()) {
            // Entities with the same update type are grouped
            return lhs.transformUpdateType() < rhs.transformUpdateType();

        } else if (lhs.getMesh()->getResourceId() != rhs.getMesh()->getResourceId()) {
            // Prioritise sorting by mesh. All entities with the same mesh are grouped together
            return lhs.getMesh()->getResourceId() < rhs.getMesh()->getResourceId();

        } else {
            // If the mesh is the same, group by material.
            return lhs.getMaterial()->getResourceId() < rhs.getMaterial()->getResourceId();
        }
    };

    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    if (m_numAddedRenderEntities > 0) {
        if (m_numAddedRenderEntities > 50) {
            // Many entities added at once, std_sort is faster
            renderEntities.sort<RenderComponent>(renderComponentComp, entt::std_sort{});
        } else {
            // Only a few entities added, insertion_sort is faster
            renderEntities.sort<RenderComponent>(renderComponentComp, entt::insertion_sort{});
        }

        std::vector<GPUObjectData> sortedObjectBuffer;
        sortedObjectBuffer.resize(m_objectDataBuffer.size());

        uint32_t index = 0;
        for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++index) {
            RenderInfo& renderInfo = renderEntities.get<RenderInfo>(*it);
            assert(renderInfo.objectIndex < m_objectDataBuffer.size());
            sortedObjectBuffer[index] = m_objectDataBuffer[renderInfo.objectIndex];
            renderInfo.objectIndex = index;
        }

        m_objectDataBuffer.swap(sortedObjectBuffer);
    }

//    LOG_INFO("Sort render entities: %u / %u", numTrue, numFalse);
}

void SceneRenderer::updateEntityWorldTransforms() {
    PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    assert(m_objectDataBuffer.size() >= renderEntities.size());

    // TODO: not update transforms for entities that never move.
    // TODO: calculate transforms for entity hierarchy.

    size_t index = 0;
    for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++index) {
        Transform& transform = renderEntities.get<Transform>(*it);
        m_objectDataBuffer[index].prevModelMatrix = m_objectDataBuffer[index].modelMatrix;
        Transform::fillMatrixf(transform, m_objectDataBuffer[index].modelMatrix);
    }
}

void SceneRenderer::updateEntityMaterials() {
    PROFILE_SCOPE("SceneRenderer::updateEntityMaterials")

    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    size_t index = 0;
    for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++index) {
        RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);
        RenderInfo& renderInfo = renderEntities.get<RenderInfo>(*it);

        if (renderComponent.getMaterial() == nullptr) {
            if (renderInfo.materialIndex != 0) {
                m_objectDataBuffer[index].materialIndex = 0;
                renderInfo.materialIndex = 0;
                renderInfo.materialId = 0;
            }
        } else if (renderInfo.materialIndex == UINT_MAX || renderInfo.materialId != renderComponent.getMaterial()->getResourceId()) {
            renderInfo.materialIndex = registerMaterial(renderComponent.getMaterial().get());
            renderInfo.materialId = renderComponent.getMaterial()->getResourceId();
            m_objectDataBuffer[index].materialIndex = renderInfo.materialIndex;
        }
    }

    if (m_resources->updateTextureDescriptorStartIndex != UINT32_MAX) {
        uint32_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->getBinding(0).descriptorCount;
        uint32_t arrayCount = (m_resources->updateTextureDescriptorEndIndex - m_resources->updateTextureDescriptorStartIndex) + 1;
        arrayCount = glm::min(arrayCount, descriptorCount);

//        LOG_INFO("Writing texture descriptors [%u to %u - %u textures]", m_resources->updateTextureDescriptorStartIndex, m_resources->updateTextureDescriptorEndIndex, arrayCount);

        if (arrayCount > 0) {
            DescriptorSetWriter(m_resources->materialDescriptorSet)
                    .writeImage(0, &m_textures[m_resources->updateTextureDescriptorStartIndex], vk::ImageLayout::eShaderReadOnlyOptimal, m_resources->updateTextureDescriptorStartIndex, arrayCount)
                    .write();
        }
    }

    m_resources->updateTextureDescriptorStartIndex = UINT32_MAX;
    m_resources->updateTextureDescriptorEndIndex = 0;
}

void SceneRenderer::streamEntityRenderData() {
    PROFILE_SCOPE("SceneRenderer::streamEntityRenderData")

    if (m_numRenderEntities > 0) {
        PROFILE_REGION("Copy object data");
        GPUObjectData* mappedObjectDataBuffer = static_cast<GPUObjectData*>(mapObjectDataBuffer(m_numRenderEntities));
        memcpy(&mappedObjectDataBuffer[0], &m_objectDataBuffer[0], m_objectDataBuffer.size() * sizeof(GPUObjectData));
    }

    if (!m_materialDataBuffer.empty()) {
        PROFILE_REGION("Copy material data");
        GPUMaterial* mappedMaterialDataBuffer = static_cast<GPUMaterial*>(mapMaterialDataBuffer(m_materialDataBuffer.size()));
        memcpy(&mappedMaterialDataBuffer[0], &m_materialDataBuffer[0], m_materialDataBuffer.size() * sizeof(GPUMaterial));
    }
}

void* SceneRenderer::mapObjectIndicesBuffer(size_t maxObjects) {

    maxObjects = CEIL_TO_MULTIPLE(maxObjects, 4);

    vk::DeviceSize newBufferSize = sizeof(uint32_t) * maxObjects;

    if (m_resources->objectIndicesBuffer == nullptr || newBufferSize > m_resources->objectIndicesBuffer->getSize()) {

        BufferConfiguration objectIndicesBufferConfig{};
        objectIndicesBufferConfig.device = Engine::graphics()->getDevice();
        objectIndicesBufferConfig.size = newBufferSize;
        objectIndicesBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        objectIndicesBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        delete m_resources->objectIndicesBuffer;
        m_resources->objectIndicesBuffer = Buffer::create(objectIndicesBufferConfig, "SceneRenderer-ObjectIndicesBuffer");

        DescriptorSetWriter(m_resources->objectDescriptorSet)
                .writeBuffer(1, m_resources->objectIndicesBuffer, 0, newBufferSize)
                .write();
    }

    void* mappedBuffer = m_resources->objectIndicesBuffer->map();
    return mappedBuffer;
}

void* SceneRenderer::mapObjectDataBuffer(size_t maxObjects) {

    vk::DeviceSize newBufferSize = sizeof(GPUObjectData) * maxObjects;

    if (m_resources->worldTransformBuffer == nullptr || newBufferSize > m_resources->worldTransformBuffer->getSize()) {

        BufferConfiguration worldTransformBufferConfig{};
        worldTransformBufferConfig.device = Engine::graphics()->getDevice();
        worldTransformBufferConfig.size = newBufferSize;
        worldTransformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        worldTransformBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        delete m_resources->worldTransformBuffer;
        m_resources->worldTransformBuffer = Buffer::create(worldTransformBufferConfig, "SceneRenderer-WorldTransformBuffer");

        DescriptorSetWriter(m_resources->objectDescriptorSet)
                .writeBuffer(0, m_resources->worldTransformBuffer, 0, newBufferSize)
                .write();
    }

    void* mappedBuffer = m_resources->worldTransformBuffer->map();
    return mappedBuffer;
}

void* SceneRenderer::mapMaterialDataBuffer(size_t maxObjects) {

    vk::DeviceSize newBufferSize = sizeof(GPUMaterial) * maxObjects;

    if (m_resources->materialDataBuffer == nullptr || newBufferSize > m_resources->materialDataBuffer->getSize()) {

        BufferConfiguration materialDataBufferConfig{};
        materialDataBufferConfig.device = Engine::graphics()->getDevice();
        materialDataBufferConfig.size = newBufferSize;
        materialDataBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        materialDataBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        delete m_resources->materialDataBuffer;
        m_resources->materialDataBuffer = Buffer::create(materialDataBufferConfig, "SceneRenderer-MaterialDataBuffer");

        DescriptorSetWriter(m_resources->materialDescriptorSet)
                .writeBuffer(1, m_resources->materialDataBuffer, 0, newBufferSize)
                .write();
    }

    void* mappedBuffer = m_resources->materialDataBuffer->map();
    return mappedBuffer;
}