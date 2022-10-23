#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Texture.h"
#include "core/util/Profiler.h"

#define UNIFORM_BUFFER_BINDING 0
#define FRAME_TEXTURE_BINDING 1

PostProcessRenderer::PostProcessRenderer() {

}

PostProcessRenderer::~PostProcessRenderer() {
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->descriptorSet;
        delete m_resources[i]->uniformBuffer;
    }
    Engine::eventDispatcher()->disconnect(&PostProcessRenderer::recreateSwapchain, this);
}

bool PostProcessRenderer::init() {
    m_graphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    m_postProcessingDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(FRAME_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build("PostProcessRenderer-PostProcessingDescriptorSetLayout");


    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;
    m_frameSampler = Sampler::get(samplerConfig, "PostProcess-FrameSampler");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        BufferConfiguration uniformBufferConfig{};
        uniformBufferConfig.device = Engine::graphics()->getDevice();
        uniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        uniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        uniformBufferConfig.size = sizeof(PostProcessUniformData);
        m_resources[i]->uniformBuffer = Buffer::create(uniformBufferConfig, "PostProcessRenderer-UniformBuffer");

        m_resources[i]->descriptorSet = DescriptorSet::create(m_postProcessingDescriptorSetLayout, descriptorPool, "PostProcessRenderer-DescriptorSet");

        DescriptorSetWriter(m_resources[i]->descriptorSet)
                .writeBuffer(UNIFORM_BUFFER_BINDING, m_resources[i]->uniformBuffer, 0, m_resources[i]->uniformBuffer->getSize())
                .write();
    }

    Engine::eventDispatcher()->connect(&PostProcessRenderer::recreateSwapchain, this);
    return true;
}

void PostProcessRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("PostProcessRenderer::render")
    BEGIN_CMD_LABEL(commandBuffer, "PostProcessRenderer::render")

    bool updateFrameTextureBinding = true;

    if (updateFrameTextureBinding) {
        ImageView* frameImageView = Engine::reprojectionRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(FRAME_TEXTURE_BINDING, m_frameSampler.get(), frameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    m_resources->uniformBuffer->upload(0, sizeof(PostProcessUniformData), &m_uniformData);

    m_graphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline->getPipelineLayout(), 0, descriptorSets, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    END_CMD_LABEL(commandBuffer)
}

void PostProcessRenderer::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    const Framebuffer* framebuffer = Engine::graphics()->getCurrentFramebuffer();
    Engine::graphics()->renderPass()->begin(commandBuffer, framebuffer, subpassContents);
}

void PostProcessRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    createGraphicsPipeline();
}

bool PostProcessRenderer::createGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = Engine::graphics()->renderPass();
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/postprocess/postprocess.frag";
    pipelineConfig.addDescriptorSetLayout(m_postProcessingDescriptorSetLayout.get());
    return m_graphicsPipeline->recreate(pipelineConfig, "PostProcessRenderer-GraphicsPipeline");
}