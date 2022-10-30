#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DescriptorSet.h"
#include "core/util/Util.h"

#define UNIFORM_BUFFER_BINDING 0
#define FRAME_TEXTURE_BINDING 1
#define VELOCITY_TEXTURE_BINDING 2
#define DEPTH_TEXTURE_BINDING 3
#define PREVIOUS_FRAME_TEXTURE_BINDING 4
#define PREVIOUS_VELOCITY_TEXTURE_BINDING 5

ReprojectionRenderer::ReprojectionRenderer():
    m_uniformData(ReprojectionUniformData{}) {
    m_uniformData.taaPreviousJitterOffset = glm::vec2(0.0F, 0.0F);
    m_uniformData.taaCurrentJitterOffset = glm::vec2(0.0F, 0.0F);
    setTaaHistoryFactor(0.1F);
    setTaaUseCatmullRomFilter(true);
    setTaaUseMitchellFilter(false);
    setTaaColourClippingMode(ColourClippingMode_Accurate);
    setTaaMitchellFilterCoefficients(0.3F, 0.3F);
    setTaaEnabled(true);
}

ReprojectionRenderer::~ReprojectionRenderer() {
    if (CONCURRENT_FRAMES == 1) {
        delete m_previousFrame.framebuffer;
        delete m_previousFrame.imageView;
        delete m_previousFrame.image;
    }
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->reprojectionDescriptorSet;
        delete m_resources[i]->reprojectionUniformBuffer;
        delete m_resources[i]->frame.framebuffer;
        delete m_resources[i]->frame.imageView;
        delete m_resources[i]->frame.image;
    }

    Engine::eventDispatcher()->disconnect(&ReprojectionRenderer::recreateSwapchain, this);
}

bool ReprojectionRenderer::init() {
    m_reprojectionGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    m_reprojectionDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(FRAME_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(VELOCITY_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(DEPTH_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(PREVIOUS_FRAME_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(PREVIOUS_VELOCITY_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build("ReprojectionRenderer-ReprojectionDescriptorSetLayout");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        bufferConfig.size = sizeof(ReprojectionUniformData);
        m_resources[i]->reprojectionUniformBuffer = Buffer::create(bufferConfig, "ReprojectionRenderer-ReprojectionUniformBuffer");

        m_resources[i]->reprojectionDescriptorSet = DescriptorSet::create(m_reprojectionDescriptorSetLayout, descriptorPool, "ReprojectionRenderer-ReprojectionDescriptorSet");
        DescriptorSetWriter(m_resources[i]->reprojectionDescriptorSet)
                .writeBuffer(UNIFORM_BUFFER_BINDING, m_resources[i]->reprojectionUniformBuffer)
                .write();

        m_resources[i]->updateDescriptorSet = true;
    }

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;
    m_frameSampler = Sampler::get(samplerConfig, "Reprojection-FrameSampler");

    if (!createRenderPass()) {
        printf("Failed to create ReprojectionRenderer RenderPass\n");
        return false;
    }

    setTaaJitterSampleCount(32);

    Engine::eventDispatcher()->connect(&ReprojectionRenderer::recreateSwapchain, this);
    return true;
}

void ReprojectionRenderer::preRender(const double& dt) {
    swapFrames();
}

void ReprojectionRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("ReprojectionRenderer::render")

    m_uniformData.resolution = Engine::graphics()->getResolution();
    if (isTaaEnabled() && !m_haltonSequence.empty()) {
        m_uniformData.taaPreviousJitterOffset = m_uniformData.taaCurrentJitterOffset;
        m_uniformData.taaCurrentJitterOffset = m_haltonSequence[Engine::currentFrameCount() % m_haltonSequence.size()] * Engine::graphics()->getNormalizedPixelSize() * 0.5F;
    } else {
        m_uniformData.taaPreviousJitterOffset = m_uniformData.taaCurrentJitterOffset = glm::vec2(0.0F, 0.0F);
    }

    ImageView* frameImageView = Engine::deferredRenderer()->getOutputFrameImageView();
    ImageView* velocityImageView = Engine::deferredRenderer()->getVelocityImageView();
    ImageView* depthImageView = Engine::deferredRenderer()->getDepthImageView();
    ImageView* prevFrameImageView = getPreviousFrameImageView();
    ImageView* prevVelocityImageView = Engine::deferredRenderer()->getPreviousVelocityImageView();

    DescriptorSetWriter descriptorSetWriter(m_resources->reprojectionDescriptorSet);
    descriptorSetWriter.writeImage(FRAME_TEXTURE_BINDING, m_frameSampler.get(), frameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    descriptorSetWriter.writeImage(VELOCITY_TEXTURE_BINDING, m_frameSampler.get(), velocityImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    descriptorSetWriter.writeImage(DEPTH_TEXTURE_BINDING, m_frameSampler.get(), depthImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    descriptorSetWriter.writeImage(PREVIOUS_FRAME_TEXTURE_BINDING, m_frameSampler.get(), prevFrameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    descriptorSetWriter.writeImage(PREVIOUS_VELOCITY_TEXTURE_BINDING, m_frameSampler.get(), prevVelocityImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    descriptorSetWriter.write();

    PROFILE_BEGIN_GPU_CMD("ReprojectionRenderer::render", commandBuffer)

    m_reprojectionGraphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->reprojectionDescriptorSet->getDescriptorSet()
    };

    m_resources->reprojectionUniformBuffer->upload(0, sizeof(ReprojectionUniformData), &m_uniformData);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_reprojectionGraphicsPipeline->getPipelineLayout(), 0, descriptorSets, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    PROFILE_END_GPU_CMD(commandBuffer)

    m_resources->frame.rendered = true;

}

void ReprojectionRenderer::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    m_renderPass->begin(commandBuffer, m_resources->frame.framebuffer, subpassContents);
}

ImageView* ReprojectionRenderer::getOutputFrameImageView() const {
    return m_resources->frame.imageView;
}

ImageView* ReprojectionRenderer::getPreviousFrameImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageView : Engine::deferredRenderer()->getAlbedoImageView();
}

bool ReprojectionRenderer::hasPreviousFrame() const {
    return m_previousFrame.rendered;
}

float ReprojectionRenderer::getTaaHistoryFactor() const {
    return m_uniformData.taaHistoryFadeFactor;
}

void ReprojectionRenderer::setTaaHistoryFactor(const float& taaHistoryFactor) {
    m_uniformData.taaHistoryFadeFactor = taaHistoryFactor;
}

bool ReprojectionRenderer::getTaaUseCatmullRomFilter() const {
    return m_uniformData.useCatmullRomFilter;
}

void ReprojectionRenderer::setTaaUseCatmullRomFilter(const bool& useCatmullRomFilter) {
    m_uniformData.useCatmullRomFilter = useCatmullRomFilter;
}

ReprojectionRenderer::ColourClippingMode ReprojectionRenderer::getTaaColourClippingMode() const {
    return (ColourClippingMode)m_uniformData.colourClippingMode;
}

void ReprojectionRenderer::setTaaColourClippingMode(const ColourClippingMode& colourClippingMode) {
    m_uniformData.colourClippingMode = (uint32_t)colourClippingMode;
}

glm::vec2 ReprojectionRenderer::getTaaMitchellFilterCoefficients() const {
    return glm::vec2(m_uniformData.mitchellB, m_uniformData.mitchellC);
}

void ReprojectionRenderer::setTaaMitchellFilterCoefficients(const float& B, const float& C) {
    m_uniformData.mitchellB = B;
    m_uniformData.mitchellC = C;
}

bool ReprojectionRenderer::getTaaUseMitchellFilter() const {
    return m_uniformData.useMitchellFilter;
}

void ReprojectionRenderer::setTaaUseMitchellFilter(const bool& useMitchellFilter) {
    m_uniformData.useMitchellFilter = useMitchellFilter;
}

bool ReprojectionRenderer::isTaaEnabled() const {
    return m_uniformData.taaEnabled;
}

void ReprojectionRenderer::setTaaEnabled(const bool& taaEnabled) {
    m_uniformData.taaEnabled = taaEnabled;
}

const glm::vec2& ReprojectionRenderer::getTaaPreviousJitterOffset() const {
    return m_uniformData.taaPreviousJitterOffset;
}

const glm::vec2& ReprojectionRenderer::getTaaCurrentJitterOffset() const {
    return m_uniformData.taaCurrentJitterOffset;
}

void ReprojectionRenderer::setTaaJitterSampleCount(const uint32_t& sampleCount) {
    m_haltonSequence.resize(sampleCount);
    for (uint32_t i = 0; i < sampleCount; ++i) {
        m_haltonSequence[i].x = Util::createHaltonSequence<float>(i + 1, 2) * 2.0F - 1.0F;
        m_haltonSequence[i].y = Util::createHaltonSequence<float>(i + 1, 3) * 2.0F - 1.0F;
    }
}

void ReprojectionRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        bool success = createFramebuffer(&m_resources[i]->frame);
        assert(success);
        m_resources[i]->updateDescriptorSet = true;
    }

    bool success;

    if (CONCURRENT_FRAMES == 1) {
        // Allocate frame date for the previous frame if there is only one concurrent frame. Otherwise, m_previousFrame
        // simply contains the existing pointers of the frame data at the index of the previous frame
        success = createFramebuffer(&m_previousFrame);
        assert(success);
    } else {
        m_previousFrame.image = nullptr;
        m_previousFrame.imageView = nullptr;
        m_previousFrame.framebuffer = nullptr;
        m_previousFrame.rendered = false;
    }

    success = createReprojectionGraphicsPipeline();
    assert(success);
    m_frameIndices.clear();
}

bool ReprojectionRenderer::createFramebuffer(FrameImages* frame) {
    delete frame->framebuffer;
    delete frame->imageView;
    delete frame->image;
    frame->rendered = false;

    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = sampleCount;
    imageConfig.setSize(Engine::graphics()->getResolution());

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();

    // Input lighting RGB image
    imageConfig.format = Engine::deferredRenderer()->getOutputColourFormat();
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    frame->image = Image2D::create(imageConfig, "ReprojectionRenderer-FrameImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->image);
    frame->imageView = ImageView::create(imageViewConfig, "ReprojectionRenderer-FrameImageView");

    // Framebuffer
    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = Engine::graphics()->getDevice();
    framebufferConfig.setSize(Engine::graphics()->getResolution());
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.addAttachment(frame->imageView);

    frame->framebuffer = Framebuffer::create(framebufferConfig, "ReprojectionRenderer-Framebuffer");
    if (frame->framebuffer == nullptr)
        return false;
    return true;
}

bool ReprojectionRenderer::createReprojectionGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.subpass = 0;
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
    pipelineConfig.depthTestEnabled = false;
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/postprocess/reprojection.frag";
    pipelineConfig.addDescriptorSetLayout(m_reprojectionDescriptorSetLayout.get());
//    pipelineConfig.addDescriptorSetLayout(Engine::lightRenderer()->getLightingRenderPassDescriptorSetLayout().get());
    return m_reprojectionGraphicsPipeline->recreate(pipelineConfig, "ReprojectionRenderer-ReprojectionGraphicsPipeline");
}

bool ReprojectionRenderer::createRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, 1> attachments;

    attachments[0].setFormat(Engine::deferredRenderer()->getOutputColourFormat());
    attachments[0].setSamples(samples);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[0].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<SubpassConfiguration, 1> subpassConfigurations;
    subpassConfigurations[0].addColourAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.setSubpasses(subpassConfigurations);
    renderPassConfig.setSubpassDependencies(dependencies);
    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));

    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig, "ReprojectionRenderer-RenderPass"));
    return (bool)m_renderPass;
}

void ReprojectionRenderer::swapFrames() {
    if (CONCURRENT_FRAMES == 1) {
        // Previous frame was allocated separately from the per-frame resources (since there is only one), so we need to swap the contents
        std::swap(m_previousFrame.image, m_resources->frame.image);
        std::swap(m_previousFrame.imageView, m_resources->frame.imageView);
        std::swap(m_previousFrame.framebuffer, m_resources->frame.framebuffer);
        std::swap(m_previousFrame.rendered, m_resources->frame.rendered);
    } else {
        // Previous frame simply points into the frame resources internal array, so we update the pointers
        m_previousFrame.image = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.image;
        m_previousFrame.imageView = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.imageView;
        m_previousFrame.framebuffer = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.framebuffer;
        m_previousFrame.rendered = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.rendered;
    }
}