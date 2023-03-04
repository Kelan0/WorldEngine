#include "core/engine/renderer/SceneRenderer.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/engine/renderer/Material.h"


struct DrawCommand {
    Mesh* mesh = nullptr;
    uint32_t instanceCount = 0;
    uint32_t firstInstance = 0;
};

struct RenderInfo {
    ResourceId materialId;
    uint32_t materialIndex;
};


SceneRenderer::SceneRenderer():
    m_scene(nullptr),
    m_numRenderEntities(0),
    m_previousPartialTicks(1.0) {
}

SceneRenderer::~SceneRenderer() {
    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->worldTransformBuffer;
        delete m_resources[i]->materialDataBuffer;
        delete m_resources[i]->objectDescriptorSet;
        delete m_resources[i]->materialDescriptorSet;
    }
}

bool SceneRenderer::init() {


    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    constexpr size_t maxTextures = 0xFFFF;

    initMissingTextureMaterial();
    std::vector<Texture*> initialTextures;
    initialTextures.resize(maxTextures, m_missingTextureMaterial->getAlbedoMap().get());

    std::vector<vk::ImageLayout> initialImageLayouts;
    initialImageLayouts.resize(maxTextures, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_objectDescriptorSetLayout = builder
            .addStorageBuffer(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .build("SceneRenderer-ObjectDescriptorSetLayout");

    m_materialDescriptorSetLayout = builder
            .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, maxTextures)
            .addStorageBuffer(1, vk::ShaderStageFlagBits::eFragment)
            .build("SceneRenderer-MaterialDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->objectDescriptorSet = DescriptorSet::create(m_objectDescriptorSetLayout, descriptorPool, "SceneRenderer-ObjectDescriptorSet");
        m_resources[i]->materialDescriptorSet = DescriptorSet::create(m_materialDescriptorSetLayout, descriptorPool, "SceneRenderer-MaterialDescriptorSet");

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
    m_materialBuffer.clear();
    registerMaterial(m_missingTextureMaterial.get());

    m_scene->enableEvents<RenderComponent>();
    m_scene->getEventDispatcher()->connect<ComponentAddedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentAdded, this);
    m_scene->getEventDispatcher()->connect<ComponentRemovedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentRemoved, this);

    m_previousPartialTicks = 1.0; // First frame considers one tick to have passed (m_previousPartialTicks > current partialTicks)
    return true;
}

void SceneRenderer::preRender(const double& dt) {
    PROFILE_SCOPE("SceneRenderer::preRender")

    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();
    m_numRenderEntities = (uint32_t)renderEntities.size();

    sortRenderEntities();
    updateEntityWorldTransforms();
    updateEntityMaterials();
    streamEntityRenderData();

    m_previousPartialTicks = Engine::instance()->getPartialTicks();
}

void SceneRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("SceneRenderer::render")
    PROFILE_BEGIN_GPU_CMD("SceneRenderer::render", commandBuffer);

    recordRenderCommands(dt, commandBuffer);

    PROFILE_END_GPU_CMD(commandBuffer);
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
        materialIndex = (uint32_t)m_materialBuffer.size();

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
        m_materialBuffer.emplace_back(gpuMaterial);

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
    RenderInfo& renderInfo = event->entity.addComponent<RenderInfo>();
    renderInfo.materialIndex = UINT32_MAX;
    renderInfo.materialId = 0;
}

void SceneRenderer::onRenderComponentRemoved(ComponentRemovedEvent<RenderComponent>* event) {
    event->entity.removeComponent<RenderInfo>();
}

void SceneRenderer::recordRenderCommands(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("SceneRenderer::recordRenderCommands");

    PROFILE_REGION("Gather DrawCommands")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    uint32_t rangeStart = 0;
    uint32_t rangeEnd = (uint32_t)renderEntities.size();

    std::vector<DrawCommand> drawCommands;

    DrawCommand currDrawCommand{};
    currDrawCommand.firstInstance = (uint32_t)rangeStart;

    auto it = renderEntities.begin() + (uint32_t)rangeStart;
    for (uint32_t index = rangeStart; index != rangeEnd; ++index, ++it) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);

        if (currDrawCommand.mesh == nullptr) {
            currDrawCommand.mesh = renderComponent.mesh().get();
            currDrawCommand.instanceCount = 0;

        } else if (currDrawCommand.mesh != renderComponent.mesh().get()) {
            drawCommands.emplace_back(currDrawCommand);
            currDrawCommand.mesh = renderComponent.mesh().get();
            currDrawCommand.firstInstance += currDrawCommand.instanceCount;
            currDrawCommand.instanceCount = 0;
        }

        ++currDrawCommand.instanceCount;
    }
    drawCommands.emplace_back(currDrawCommand);

    PROFILE_REGION("Draw meshes")
    for (auto& command : drawCommands)
        command.mesh->draw(commandBuffer, command.instanceCount, command.firstInstance);

    PROFILE_END_REGION()

    PROFILE_BEGIN_GPU_CMD("SceneRenderer::recordRenderCommands", commandBuffer);
    PROFILE_END_GPU_CMD(commandBuffer);
}

void SceneRenderer::sortRenderEntities() {
    PROFILE_SCOPE("SceneRenderer::sortRenderEntities")

    auto renderComponentComp = [](const RenderComponent& lhs, const RenderComponent& rhs) {
        if (lhs.mesh()->getResourceId() != rhs.mesh()->getResourceId()) {
            // Prioritise sorting by mesh. All entities with the same mesh are grouped together
            return lhs.mesh()->getResourceId() < rhs.mesh()->getResourceId();
        } else {
            // If the mesh is the same, group by material.
            return lhs.material()->getResourceId() < rhs.material()->getResourceId();
        }
    };


    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();
    renderEntities.sort([&renderEntities, &renderComponentComp](const entt::entity &lhs, const entt::entity &rhs) {
        return renderComponentComp(renderEntities.get<RenderComponent>(lhs), renderEntities.get<RenderComponent>(rhs));
    }, entt::std_sort{});
}

void SceneRenderer::updateEntityWorldTransforms() {
    PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    m_objectBuffer.resize(renderEntities.size());

    size_t index = 0;

    for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++index) {
        RenderInfo& renderInfo = renderEntities.get<RenderInfo>(*it);
        Transform& transform = renderEntities.get<Transform>(*it);
        m_objectBuffer[index].prevModelMatrix = m_objectBuffer[index].modelMatrix;
        Transform::fillMatrixf(transform, m_objectBuffer[index].modelMatrix);
    }
}

void SceneRenderer::updateEntityMaterials() {
    PROFILE_SCOPE("SceneRenderer::updateEntityMaterials")

    const auto& renderEntities = m_scene->registry()->group<RenderComponent, RenderInfo, Transform>();

    size_t index = 0;
    for (auto it = renderEntities.begin(); it != renderEntities.end(); ++it, ++index) {
        RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);
        RenderInfo& renderInfo = renderEntities.get<RenderInfo>(*it);

        if (renderComponent.material() == nullptr) {
            renderInfo.materialIndex = 0; // Default null material
            renderInfo.materialId = 0;
        } else if (renderInfo.materialIndex == UINT_MAX || renderInfo.materialId != renderComponent.material()->getResourceId()) {
            renderInfo.materialIndex = registerMaterial(renderComponent.material().get());
            renderInfo.materialId = renderComponent.material()->getResourceId();
        }

        m_objectBuffer[index].materialIndex = renderInfo.materialIndex;
    }

    if (m_resources->updateTextureDescriptorStartIndex != UINT32_MAX) {
        uint32_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;
        uint32_t arrayCount = (m_resources->updateTextureDescriptorEndIndex - m_resources->updateTextureDescriptorStartIndex) + 1;
        arrayCount = glm::min(arrayCount, descriptorCount);

//        printf("Writing texture descriptors [%u to %u - %u textures]\n", m_resources->updateTextureDescriptorStartIndex, m_resources->updateTextureDescriptorEndIndex, arrayCount);

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
        GPUObjectData *mappedObjectBuffer = static_cast<GPUObjectData *>(mapObjectDataBuffer(m_numRenderEntities));
        memcpy(&mappedObjectBuffer[0], &m_objectBuffer[0], m_objectBuffer.size() * sizeof(GPUObjectData));
    }

    if (!m_materialBuffer.empty()) {
        PROFILE_REGION("Copy material data");
        GPUMaterial *mappedMaterialBuffer = static_cast<GPUMaterial *>(mapMaterialDataBuffer(m_materialBuffer.size()));
        memcpy(&mappedMaterialBuffer[0], &m_materialBuffer[0], m_materialBuffer.size() * sizeof(GPUMaterial));
    }
}

void* SceneRenderer::mapObjectDataBuffer(const size_t& maxObjects) {
    PROFILE_SCOPE("SceneRenderer::mapObjectDataBuffer")

    vk::DeviceSize newBufferSize = sizeof(GPUObjectData) * maxObjects;

    if (m_resources->worldTransformBuffer == nullptr || newBufferSize > m_resources->worldTransformBuffer->getSize()) {
        PROFILE_SCOPE("Allocate WorldTransformBuffer");

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

    PROFILE_REGION("Map buffer");
    void* mappedBuffer = m_resources->worldTransformBuffer->map();
    return mappedBuffer;
}

void* SceneRenderer::mapMaterialDataBuffer(const size_t& maxObjects) {
    PROFILE_SCOPE("SceneRenderer::mapMaterialDataBuffer")

    vk::DeviceSize newBufferSize = sizeof(GPUMaterial) * maxObjects;

    if (m_resources->materialDataBuffer == nullptr || newBufferSize > m_resources->materialDataBuffer->getSize()) {
        PROFILE_SCOPE("Allocate materialDataBuffer");

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

    PROFILE_REGION("Map buffer");
    void* mappedBuffer = m_resources->materialDataBuffer->map();
    return mappedBuffer;
}