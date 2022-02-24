#include "SceneRenderer.h"
#include "RenderCamera.h"
#include "RenderComponent.h"
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

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_globalDescriptorSet[i].reset();
        m_objectDescriptorSet[i].reset();
        m_materialDescriptorSet[i].reset();
    }
}

void SceneRenderer::init() {
    initMissingTexture();

    std::shared_ptr<DescriptorPool> descriptorPool = Application::instance()->graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    auto globalDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(CameraInfoUBO)).build();
    auto objectDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(WorldTransformUBO)).build();
    auto materialDescriptorSetLayout = builder.addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, 1).build();
        
    globalDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_globalDescriptorSet.data());
    objectDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_objectDescriptorSet.data());
    materialDescriptorSetLayout->createDescriptorSets(descriptorPool, CONCURRENT_FRAMES, m_materialDescriptorSet.data());

    BufferConfiguration cameraInfoBufferConfig;
    cameraInfoBufferConfig.device = Application::instance()->graphics()->getDevice();
    cameraInfoBufferConfig.size = sizeof(CameraInfoUBO);
    cameraInfoBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    cameraInfoBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

    BufferConfiguration worldTransformBufferConfig;
    worldTransformBufferConfig.device = Application::instance()->graphics()->getDevice();
    worldTransformBufferConfig.size = sizeof(WorldTransformUBO);
    worldTransformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    worldTransformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

    FrameResource<Buffer>::create(m_cameraInfoBuffer, cameraInfoBufferConfig);
    FrameResource<Buffer>::create(m_worldTransformBuffer, worldTransformBufferConfig);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        DescriptorSetWriter(m_globalDescriptorSet[i]).writeBuffer(0, m_cameraInfoBuffer[i], 0, sizeof(CameraInfoUBO)).write();
        DescriptorSetWriter(m_objectDescriptorSet[i]).writeBuffer(0, m_worldTransformBuffer[i], 0, sizeof(WorldTransformUBO)).write();
    }
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

    const vk::CommandBuffer& commandBuffer = Application::instance()->graphics()->getCurrentCommandBuffer();
    recordRenderCommands(dt, commandBuffer);
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}

void SceneRenderer::recordRenderCommands(double dt, vk::CommandBuffer commandBuffer) {
    GraphicsManager* graphics = Application::instance()->graphics();
    const uint32_t currentFrameIndex = graphics->getCurrentFrameIndex();

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    std::shared_ptr<GraphicsPipeline> pipeline = Application::instance()->graphics()->pipeline();

    for (auto id : renderEntities) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);
        const Transform& transform = renderEntities.get<Transform>(id);

        std::shared_ptr<Texture2D> texture = renderComponent.texture;
        if (texture == NULL)
            texture = m_missingTexture;

        DescriptorSetWriter(m_materialDescriptorSet[currentFrameIndex])
            .writeImage(0, texture.get(), vk::ImageLayout::eShaderReadOnlyOptimal).write();

        glm::mat4 modelMatrix = transform.getMatrix();
        m_worldTransformBuffer[currentFrameIndex]->upload(0, sizeof(glm::mat4), &modelMatrix);

        std::vector<vk::DescriptorSet> descriptorSets = {
            m_globalDescriptorSet[currentFrameIndex]->getDescriptorSet(),
            m_objectDescriptorSet[currentFrameIndex]->getDescriptorSet(),
            m_materialDescriptorSet[currentFrameIndex]->getDescriptorSet(),
        };

        std::vector<uint32_t> dynamicOffsets = {};

        graphics->pipeline()->bind(commandBuffer);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics->pipeline()->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);
        renderComponent.mesh->draw(commandBuffer);
    }
}

std::shared_ptr<DescriptorSet> SceneRenderer::globalDescriptorSet() {
    return m_globalDescriptorSet[Application::instance()->graphics()->getCurrentFrameIndex()];
}

std::shared_ptr<DescriptorSet> SceneRenderer::objectDescriptorSet() {
    return m_objectDescriptorSet[Application::instance()->graphics()->getCurrentFrameIndex()];
}

std::shared_ptr<DescriptorSet> SceneRenderer::materialDescriptorSet() {
    return m_materialDescriptorSet[Application::instance()->graphics()->getCurrentFrameIndex()];
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
