#include "SceneRenderer.h"
#include "RenderCamera.h"
#include "RenderComponent.h"
#include "../scene/event/EventDispacher.h"
#include "../scene/Scene.h"
#include "../scene/Transform.h"
#include "../../application/Application.h"
#include "../../graphics/GraphicsManager.h"
#include "../../graphics/GraphicsPipeline.h"
#include "../../graphics/DescriptorSet.h"
#include "../../graphics/Buffer.h"
#include "../../graphics/Mesh.h"
#include "../../graphics/Texture.h"

SceneRenderer::SceneRenderer() {
}

SceneRenderer::~SceneRenderer() {
    m_missingTexture.reset();
    m_missingTextureImage.reset();
    m_cameraInfoBuffer.reset();
    m_worldTransformBuffer.reset();
    m_globalDescriptorSet.reset();
    m_objectDescriptorSet.reset();
    m_materialDescriptorSet.reset();
}

void SceneRenderer::init() {
    initMissingTexture();

    std::shared_ptr<DescriptorPool> descriptorPool = Application::instance()->graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    auto globalDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(CameraInfoUBO)).build();
    auto objectDescriptorSetLayout = builder.addStorageBlock(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, sizeof(ObjectDataOBO)).build();
    auto materialDescriptorSetLayout = builder.addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, 100000).build();
        
    FrameResource<DescriptorSet>::create(m_globalDescriptorSet, globalDescriptorSetLayout, descriptorPool);
    FrameResource<DescriptorSet>::create(m_objectDescriptorSet, objectDescriptorSetLayout, descriptorPool);
    FrameResource<DescriptorSet>::create(m_materialDescriptorSet, materialDescriptorSetLayout, descriptorPool);
    //globalDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_globalDescriptorSet.data());
    //objectDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_objectDescriptorSet.data());
    //materialDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_materialDescriptorSet.data());

    BufferConfiguration cameraInfoBufferConfig;
    cameraInfoBufferConfig.device = Application::instance()->graphics()->getDevice();
    cameraInfoBufferConfig.size = sizeof(CameraInfoUBO);
    cameraInfoBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    cameraInfoBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

    BufferConfiguration worldTransformBufferConfig;
    worldTransformBufferConfig.device = Application::instance()->graphics()->getDevice();
    worldTransformBufferConfig.size = sizeof(ObjectDataOBO) * 100000;
    worldTransformBufferConfig.memoryProperties = 
        vk::MemoryPropertyFlagBits::eDeviceLocal;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    worldTransformBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

    FrameResource<Buffer>::create(m_cameraInfoBuffer, cameraInfoBufferConfig);
    FrameResource<Buffer>::create(m_worldTransformBuffer, worldTransformBufferConfig);

    std::vector<Texture2D*> initialTextures;
    initialTextures.resize(100000, m_missingTexture.get());

    std::vector<vk::ImageLayout> initialImageLayouts;
    initialImageLayouts.resize(100000, vk::ImageLayout::eShaderReadOnlyOptimal);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        DescriptorSetWriter(m_globalDescriptorSet[i]).writeBuffer(0, m_cameraInfoBuffer[i], 0, m_cameraInfoBuffer->getSize()).write();
        DescriptorSetWriter(m_objectDescriptorSet[i]).writeBuffer(0, m_worldTransformBuffer[i], 0, m_worldTransformBuffer->getSize()).write();

        DescriptorSetWriter(m_materialDescriptorSet[i])
            .writeImage(0, initialTextures.data(), initialImageLayouts.data(), 100000, 0).write();
    }

    m_scene->enableEvents<RenderComponent>();
    m_scene->getEventDispacher()->connect<ComponentAddedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentAdded, this);
    m_scene->getEventDispacher()->connect<ComponentRemovedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentRemoved, this);
}

void SceneRenderer::render(double dt) {
    const Entity& cameraEntity = m_scene->getMainCamera();
    m_renderCamera.setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera.setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera.update();

    render(dt, &m_renderCamera);
}

void SceneRenderer::render(double dt, RenderCamera* renderCamera) {
    const uint32_t currentFrameIndex = Application::instance()->graphics()->getCurrentFrameIndex();

    CameraInfoUBO cameraInfo;
    cameraInfo.viewProjectionMatrix = renderCamera->getViewProjectionMatrix();
    m_cameraInfoBuffer->upload(0, sizeof(CameraInfoUBO), &cameraInfo);

    if (m_needsSortRenderableEntities) {
        m_needsSortRenderableEntities = false;
        sortRenderableEntities();
    }

    updateMaterialsBuffer();
    updateEntityWorldTransforms();

    const vk::CommandBuffer& commandBuffer = Application::instance()->graphics()->getCurrentCommandBuffer();
    recordRenderCommands(dt, commandBuffer);
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}

void SceneRenderer::initPipelineDescriptorSetLayouts(GraphicsPipelineConfiguration& graphicsPipelineConfiguration) const {
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_globalDescriptorSet->getLayout()->getDescriptorSetLayout());
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_objectDescriptorSet->getLayout()->getDescriptorSetLayout());
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_materialDescriptorSet->getLayout()->getDescriptorSetLayout());
}

void SceneRenderer::recordRenderCommands(double dt, vk::CommandBuffer commandBuffer) {
    GraphicsManager* graphics = Application::instance()->graphics();
    const uint32_t currentFrameIndex = graphics->getCurrentFrameIndex();
    const vk::Device& device = **graphics->getDevice();


    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    std::shared_ptr<GraphicsPipeline> pipeline = Application::instance()->graphics()->pipeline();


    std::vector<vk::DescriptorSet> descriptorSets = {
        m_globalDescriptorSet->getDescriptorSet(),
        m_objectDescriptorSet->getDescriptorSet(),
        m_materialDescriptorSet->getDescriptorSet(),
    };

    std::vector<uint32_t> dynamicOffsets = {};

    graphics->pipeline()->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics->pipeline()->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

    uint32_t firstInstance = 0;
    uint32_t instanceCount = 0;
    Mesh* currMesh = NULL;

    for (auto id : renderEntities) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);

        if (currMesh == NULL) {
            currMesh = renderComponent.mesh.get();
            instanceCount = 0;
            firstInstance = 0;

        } else if (currMesh != renderComponent.mesh.get()) {
            currMesh->draw(commandBuffer, instanceCount, firstInstance);
            currMesh = renderComponent.mesh.get();
            firstInstance += instanceCount;
            instanceCount = 0;
        }

        ++instanceCount;
    }

    if (currMesh != NULL) {
        currMesh->draw(commandBuffer, instanceCount, firstInstance);
    }

}

void SceneRenderer::initMissingTexture() {
    union {
        glm::u8vec4 pixels[2][2];
        uint8_t pixelBytes[sizeof(glm::u8vec4) * 4];
    };

    pixels[0][0] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);
    pixels[0][1] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][0] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][1] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);

    ImageData imageData(pixelBytes, 2, 2, ImagePixelLayout::RGBA, ImagePixelFormat::UInt8);

    Image2DConfiguration missingTextureImageConfig;
    missingTextureImageConfig.device = Application::instance()->graphics()->getDevice();
    missingTextureImageConfig.format = vk::Format::eR8G8B8A8Unorm;
    missingTextureImageConfig.imageData = &imageData;
    m_missingTextureImage = std::shared_ptr<Image2D>(Image2D::create(missingTextureImageConfig));

    ImageView2DConfiguration missingTextureImageViewConfig;
    missingTextureImageViewConfig.device = missingTextureImageConfig.device;
    missingTextureImageViewConfig.format = missingTextureImageConfig.format;
    missingTextureImageViewConfig.image = m_missingTextureImage->getImage();

    SamplerConfiguration missingTextureSamplerConfig;
    missingTextureSamplerConfig.device = missingTextureImageConfig.device;
    missingTextureSamplerConfig.minFilter = vk::Filter::eNearest;
    missingTextureSamplerConfig.magFilter = vk::Filter::eNearest;
    m_missingTexture = std::shared_ptr<Texture2D>(Texture2D::create(missingTextureImageViewConfig, missingTextureSamplerConfig));
}

void SceneRenderer::sortRenderableEntities() const {
    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    // Group all meshes of the same type to be contiguous in the array
    renderEntities.sort([renderEntities](const entt::entity& lhs, const entt::entity& rhs) {
        const RenderComponent& lhsRC = renderEntities.get<RenderComponent>(lhs);
        const RenderComponent& rhsRC = renderEntities.get<RenderComponent>(rhs);
        return lhsRC.mesh->getResourceId() < rhsRC.mesh->getResourceId();
    });
}

void SceneRenderer::updateEntityWorldTransforms() const {

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    std::vector<ObjectDataOBO> objectDataArray;

    for (auto id : renderEntities) {
        const Transform& transform = renderEntities.get<Transform>(id);
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);

        ObjectDataOBO& objectData = objectDataArray.emplace_back();
        objectData.modelMatrix = transform.getMatrix();
        objectData.textureIndex = 0;

        auto it = m_textureDescriptorIndices.find(renderComponent.texture.get());
        if (it != m_textureDescriptorIndices.end()) {
            objectData.textureIndex = it->second;
        }
    }

    size_t bufferSize = sizeof(ObjectDataOBO) * objectDataArray.size();
    m_worldTransformBuffer->upload(0, bufferSize, objectDataArray.data());
}

void SceneRenderer::updateMaterialsBuffer() {
    uint32_t descriptorCount = m_materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;

    m_textureDescriptorIndices.clear();

    std::vector<Texture2D*> textures;
    textures.reserve(descriptorCount);
    textures.emplace_back(m_missingTexture.get());
    m_textureDescriptorIndices.insert(std::make_pair(nullptr, 0)); // NULL texture = index 0

    std::vector<vk::ImageLayout> imageLayouts;
    imageLayouts.reserve(descriptorCount);
    imageLayouts.emplace_back(vk::ImageLayout::eShaderReadOnlyOptimal);

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    for (auto id : renderEntities) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);

        if (renderComponent.texture != NULL) {

            auto it = m_textureDescriptorIndices.find(renderComponent.texture.get());
            if (it == m_textureDescriptorIndices.end()) {
                uint32_t index = textures.size();
                m_textureDescriptorIndices.insert(std::make_pair(renderComponent.texture.get(), index));
                textures.emplace_back(renderComponent.texture.get());
                imageLayouts.emplace_back(vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }
    }

    uint32_t arrayCount = glm::min((uint32_t)textures.size(), descriptorCount);
    DescriptorSetWriter(m_materialDescriptorSet.get())
        .writeImage(0, textures.data(), imageLayouts.data(), arrayCount, 0).write();
}

void SceneRenderer::onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event) {
    //printf("SceneRenderer::onRenderComponentAdded");
    m_needsSortRenderableEntities = true;
}

void SceneRenderer::onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event) {
    //printf("SceneRenderer::onRenderComponentRemoved");
    m_needsSortRenderableEntities = true;
}
