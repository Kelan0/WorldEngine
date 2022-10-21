#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/scene/event/EventDispatcher.h"
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
    delete m_frameSampler;
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
            .addCombinedImageSampler(FRAME_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment, CONCURRENT_FRAMES + 1)
            .build("PostProcessRenderer-PostProcessingDescriptorSetLayout");


    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;
    m_frameSampler = Sampler::create(samplerConfig, "PostProcessRenderer-FrameSampler");

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

//    if (!createRenderPass()) {
//        printf("Failed to create PostProcessRenderer RenderPass\n");
//        return false;
//    }

    Engine::eventDispatcher()->connect(&PostProcessRenderer::recreateSwapchain, this);
    return true;
}

void PostProcessRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("PostProcessRenderer::render")
    BEGIN_CMD_LABEL(commandBuffer, "PostProcessRenderer::render")

    bool updateFrameTextureBinding = false;

    ImageView* currentFrameImageView = Engine::deferredLightingPass()->getCurrentFrameImageView();
    auto [currFrameIt, currFrameUpdated] = m_frameIndices.insert(std::make_pair(currentFrameImageView, (int32_t)m_frameIndices.size()));
    m_uniformData.currentFrameIndex = currFrameIt->second;
    updateFrameTextureBinding |= currFrameUpdated;

    if (Engine::deferredLightingPass()->hasPreviousFrame()) {
        ImageView* previousFrameImageView = Engine::deferredLightingPass()->getPreviousFrameImageView();
        auto [prevFrameIt, prevFrameUpdated] = m_frameIndices.insert(std::make_pair(previousFrameImageView, (int32_t)m_frameIndices.size()));
        m_uniformData.previousFrameIndex = prevFrameIt->second;
        updateFrameTextureBinding |= prevFrameUpdated;
    } else {
        m_uniformData.previousFrameIndex = m_uniformData.currentFrameIndex;
    }

    if (updateFrameTextureBinding) {
        std::array<ImageView*, CONCURRENT_FRAMES + 1> frameTextureImageViews{};
        std::fill(frameTextureImageViews.begin(), frameTextureImageViews.end(), currentFrameImageView);
        for (auto& [imageView, index] : m_frameIndices)
            frameTextureImageViews[index] = imageView;

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(FRAME_TEXTURE_BINDING, m_frameSampler, frameTextureImageViews.data(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, (uint32_t)frameTextureImageViews.size())
                .write();
    }

    m_resources->uniformBuffer->upload(0, sizeof(PostProcessUniformData), &m_uniformData);

    m_graphicsPipeline->bind(commandBuffer);

    vk::DescriptorSet descriptorSets[1] = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline->getPipelineLayout(), 0, 1, descriptorSets, 0, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    END_CMD_LABEL(commandBuffer)
}

const float& PostProcessRenderer::getTaaHistoryFactor() const {
    return m_uniformData.taaHistoryFactor;
}

void PostProcessRenderer::setTaaHistoryFactor(const float& taaHistoryFactor) {
    m_uniformData.taaHistoryFactor = taaHistoryFactor;
}

void PostProcessRenderer::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    const Framebuffer* framebuffer = Engine::graphics()->getCurrentFramebuffer();
    Engine::graphics()->renderPass()->begin(commandBuffer, framebuffer, subpassContents);
}

void PostProcessRenderer::recreateSwapchain(const RecreateSwapchainEvent& event) {
//    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
//        createFramebuffer(&m_resources[i]->frameImage);
//    }
    createGraphicsPipeline();
    m_frameIndices.clear();
}

//bool PostProcessRenderer::createFramebuffer(FrameImage* frameImage) {
//    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
//
//    delete frameImage->framebuffer;
//    delete frameImage->imageView;
//    delete frameImage->image;
//    frameImage->rendered = false;
//
//    Image2DConfiguration imageConfig{};
//    imageConfig.device = Engine::graphics()->getDevice();
//    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
//    imageConfig.sampleCount = sampleCount;
//    imageConfig.setSize(Engine::graphics()->getResolution());
//
//    ImageViewConfiguration imageViewConfig{};
//    imageViewConfig.device = Engine::graphics()->getDevice();
//
//    imageConfig.format = Engine::graphics()->getColourFormat();
//    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;// | vk::ImageUsageFlagBits::eTransferSrc;
//    frameImage->image = Image2D::create(imageConfig, "PostProcessRenderer-FrameImage");
//    imageViewConfig.format = imageConfig.format;
//    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
//    imageViewConfig.setImage(frameImage->image);
//    frameImage->imageView = ImageView::create(imageViewConfig, "PostProcessRenderer-FrameImageView");
//
//    // Framebuffer
//    FramebufferConfiguration framebufferConfig{};
//    framebufferConfig.device = Engine::graphics()->getDevice();
//    framebufferConfig.setSize(Engine::graphics()->getResolution());
//    framebufferConfig.setRenderPass(m_renderPass.get());
//    framebufferConfig.addAttachment(frameImage->imageView);
//
//    frameImage->framebuffer = Framebuffer::create(framebufferConfig, "PostProcessRenderer-Framebuffer");
//    assert(frameImage->framebuffer != nullptr);
//    return true;
//}

//bool PostProcessRenderer::createRenderPass() {
//    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
//
//    std::array<vk::AttachmentDescription, 1> attachments;
//
//    attachments[0].setFormat(Engine::graphics()->getColourFormat());
//    attachments[0].setSamples(samples);
//    attachments[0].setLoadOp(vk::AttachmentLoadOp::eDontCare);
//    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
//    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
//    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
//    attachments[0].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
//
//    SubpassConfiguration subpassConfiguration{};
//    subpassConfiguration.addColourAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
//
//    std::array<vk::SubpassDependency, 2> dependencies;
//    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
//    dependencies[0].setDstSubpass(0);
//    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
//    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
//    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
//    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
//    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);
//
//    dependencies[1].setSrcSubpass(0);
//    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
//    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
//    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
//    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
//    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
//    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);
//
//    RenderPassConfiguration renderPassConfig{};
//    renderPassConfig.device = Engine::graphics()->getDevice();
//    renderPassConfig.setAttachments(attachments);
//    renderPassConfig.setSubpasses(subpassConfiguration);
//    renderPassConfig.setSubpassDependencies(dependencies);
//
//    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig, "PostProcessRenderer-RenderPass"));
//    return (bool)m_renderPass;
//}

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