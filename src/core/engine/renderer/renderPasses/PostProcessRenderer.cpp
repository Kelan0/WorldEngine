#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Texture.h"
#include "core/util/Profiler.h"

#define POSTPROCESS_UNIFORM_BUFFER_BINDING 0
#define POSTPROCESS_FRAME_TEXTURE_BINDING 1
#define POSTPROCESS_BLOOM_TEXTURE_BINDING 2
#define BLOOM_BLUR_UNIFORM_BUFFER_BINDING 0
#define BLOOM_BLUR_SRC_TEXTURE_BINDING 1

PostProcessRenderer::PostProcessRenderer():
    m_postProcessUniformData(PostProcessUniformData{}),
    m_bloomBlurUniformData(BloomBlurUniformData{}),
    m_bloomBlurMaxIterations(8) {
    m_resources.initDefault();
    setBloomEnabled(true);
    setBloomBlurFilterRadius(8.0F);
    setBloomIntensity(0.05F);
    setBloomThreshold(1.0F);
    setBloomSoftThreshold(0.5F);
//    setBloomBlurIterations(5);
    m_bloomBlurIterations = 4;
}

PostProcessRenderer::~PostProcessRenderer() {
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->postProcessDescriptorSet;
        delete m_resources[i]->postProcessUniformBuffer;
        delete m_resources[i]->bloomBlurUniformBuffer;
        delete m_resources[i]->bloomBlurInputDescriptorSet;
        for (auto& descriptorSet : m_resources[i]->bloomBlurDescriptorSets)
            delete descriptorSet;
        for (auto& framebuffer : m_resources[i]->bloomBlurMipFramebuffers)
            delete framebuffer;
        for (auto& imageView : m_resources[i]->bloomBlurMipImageViews)
            delete imageView;
        delete m_resources[i]->bloomTextureImageView;
        delete m_resources[i]->bloomBlurImage;
    }
    Engine::eventDispatcher()->disconnect(&PostProcessRenderer::recreateSwapchain, this);
}

bool PostProcessRenderer::init() {
    m_postProcessGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));
    m_downsampleGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));
    m_upsampleGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    m_postProcessDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(POSTPROCESS_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(POSTPROCESS_FRAME_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(POSTPROCESS_BLOOM_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build("PostProcessRenderer-PostProcessDescriptorSetLayout");

    m_bloomBlurDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(BLOOM_BLUR_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(BLOOM_BLUR_SRC_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build("PostProcessRenderer-BloomBlurDescriptorSetLayout");

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.wrapV = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.minLod = 0.0F;
    samplerConfig.maxLod = (float)m_bloomBlurMaxIterations;
    m_frameSampler = Sampler::get(samplerConfig, "PostProcess-FrameSampler");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        BufferConfiguration uniformBufferConfig{};
        uniformBufferConfig.device = Engine::graphics()->getDevice();
        uniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        uniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        uniformBufferConfig.size = sizeof(PostProcessUniformData);
        m_resources[i]->postProcessUniformBuffer = Buffer::create(uniformBufferConfig, "PostProcessRenderer-PostProcessUniformBuffer");

        m_resources[i]->postProcessDescriptorSet = DescriptorSet::create(m_postProcessDescriptorSetLayout, descriptorPool, "PostProcessRenderer-PostProcessDescriptorSet");

        DescriptorSetWriter(m_resources[i]->postProcessDescriptorSet)
                .writeBuffer(POSTPROCESS_UNIFORM_BUFFER_BINDING, m_resources[i]->postProcessUniformBuffer, 0, m_resources[i]->postProcessUniformBuffer->getSize())
                .write();

        vk::DeviceSize alignedUniformBufferSize = Engine::graphics()->getAlignedUniformBufferOffset(sizeof(BloomBlurUniformData));
        uniformBufferConfig.size = alignedUniformBufferSize;
        m_resources[i]->bloomBlurUniformBuffer = Buffer::create(uniformBufferConfig, "PostProcessRenderer-BloomBlurUniformBuffer");

        m_resources[i]->bloomBlurDescriptorSets.resize(m_bloomBlurMaxIterations);
        for (uint32_t j = 0; j < m_bloomBlurMaxIterations; ++j) {
            DescriptorSet* descriptorSet = DescriptorSet::create(m_bloomBlurDescriptorSetLayout, descriptorPool, "PostProcessRenderer-BloomBlurDescriptorSet");
            m_resources[i]->bloomBlurDescriptorSets[j] = descriptorSet;
            DescriptorSetWriter(descriptorSet)
                    .writeBuffer(BLOOM_BLUR_UNIFORM_BUFFER_BINDING, m_resources[i]->bloomBlurUniformBuffer, 0, alignedUniformBufferSize)
                    .write();
        }
        m_resources[i]->updateInputImage = true;
        m_resources[i]->bloomBlurInputDescriptorSet = DescriptorSet::create(m_bloomBlurDescriptorSetLayout, descriptorPool, "PostProcessRenderer-BloomBlurInputDescriptorSet");
        DescriptorSetWriter(m_resources[i]->bloomBlurInputDescriptorSet)
                .writeBuffer(BLOOM_BLUR_UNIFORM_BUFFER_BINDING, m_resources[i]->bloomBlurUniformBuffer, 0, alignedUniformBufferSize)
                .write();
    }

    if (!createBloomBlurRenderPass()) {
        printf("Failed to create PostProcessRenderer BloomBlurRenderPass\n");
        return false;
    }

    Engine::eventDispatcher()->connect(&PostProcessRenderer::recreateSwapchain, this);
    return true;
}

void PostProcessRenderer::renderBloomBlur(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("PostProcessRenderer::renderBloomBlur")

    if (!m_postProcessUniformData.bloomEnabled) {
        return;
    }

    PROFILE_BEGIN_GPU_CMD("PostProcessRenderer::renderBloomBlur", commandBuffer)

    if (m_resources->bloomBlurIterations != m_bloomBlurIterations) {
        printf("Updating bloom blur iterations from %u to %u\n", m_resources->bloomBlurIterations , m_bloomBlurIterations);
        bool success = createBloomBlurFramebuffer(m_resources.get());
        assert(success);
    }

    ImageView* lightingOutputImageView = Engine::deferredLightingPass()->getOutputFrameImageView();
    if (m_resources->updateInputImage) {
        DescriptorSetWriter(m_resources->bloomBlurInputDescriptorSet)
                .writeImage(BLOOM_BLUR_SRC_TEXTURE_BINDING, m_frameSampler.get(), lightingOutputImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    if (m_resources->bloomBlurUniformDataChanged) {
        m_resources->bloomBlurUniformBuffer->upload(0, sizeof(BloomBlurUniformData), &m_bloomBlurUniformData);
    }

    BloomBlurPushConstantData pushConstantData{};

    vk::Viewport viewport;

    // Progressively down-sample
    for (uint32_t i = 0; i < m_resources->bloomBlurIterations - 1; ++i) {
        Framebuffer* framebuffer = m_resources->bloomBlurMipFramebuffers[i + 1]; // output to next mip level
        m_bloomBlurRenderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);
        m_downsampleGraphicsPipeline->setViewport(commandBuffer, 0, GraphicsPipeline::getScreenViewport(framebuffer->getResolution()));
        m_downsampleGraphicsPipeline->bind(commandBuffer);

        DescriptorSet* descriptorSet = i == 0 ? m_resources->bloomBlurInputDescriptorSet : m_resources->bloomBlurDescriptorSets[i];

        std::array<vk::DescriptorSet, 1> descriptorSets = {
                descriptorSet->getDescriptorSet()
        };

        pushConstantData.texelSize = glm::vec2(1.0F) / glm::vec2(framebuffer->getResolution());
        pushConstantData.passIndex = i;

        const vk::PipelineLayout& pipelineLayout = m_downsampleGraphicsPipeline->getPipelineLayout();
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);
        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomBlurPushConstantData), &pushConstantData);
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    // Progressively up-sample
    for (uint32_t i = m_resources->bloomBlurIterations - 1; i > 0; --i) {
        Framebuffer* framebuffer = m_resources->bloomBlurMipFramebuffers[i - 1]; // output to previous mip level
        m_bloomBlurRenderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);
        m_upsampleGraphicsPipeline->setViewport(commandBuffer, 0, GraphicsPipeline::getScreenViewport(framebuffer->getResolution()));
        m_upsampleGraphicsPipeline->bind(commandBuffer);

        DescriptorSet* descriptorSet = m_resources->bloomBlurDescriptorSets[i];

        std::array<vk::DescriptorSet, 1> descriptorSets = {
                descriptorSet->getDescriptorSet()
        };

        pushConstantData.texelSize = glm::vec2(1.0F) / glm::vec2(framebuffer->getResolution());
        pushConstantData.passIndex = i;

        const vk::PipelineLayout& pipelineLayout = m_upsampleGraphicsPipeline->getPipelineLayout();

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);
        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomBlurPushConstantData), &pushConstantData);
        commandBuffer.draw(3, 1, 0, 0);
        commandBuffer.endRenderPass();
    }

    PROFILE_END_GPU_CMD(commandBuffer)
}

void PostProcessRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("PostProcessRenderer::render")
    PROFILE_BEGIN_GPU_CMD("PostProcessRenderer::render", commandBuffer)

    if (m_resources->updateInputImage) {
        ImageView* frameImageView = Engine::reprojectionRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->postProcessDescriptorSet)
                .writeImage(POSTPROCESS_FRAME_TEXTURE_BINDING, m_frameSampler.get(), frameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    if (m_resources->postProcessUniformDataChanged) {
        m_resources->postProcessUniformBuffer->upload(0, sizeof(PostProcessUniformData), &m_postProcessUniformData);
    }

    m_postProcessGraphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->postProcessDescriptorSet->getDescriptorSet()
    };

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_postProcessGraphicsPipeline->getPipelineLayout(), 0, descriptorSets, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    PROFILE_END_GPU_CMD(commandBuffer)

    m_resources->updateInputImage = false;
}

void PostProcessRenderer::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    const Framebuffer* framebuffer = Engine::graphics()->getCurrentFramebuffer();
    Engine::graphics()->renderPass()->begin(commandBuffer, framebuffer, subpassContents);
}

bool PostProcessRenderer::isBloomEnabled() const {
    return m_postProcessUniformData.bloomEnabled;
}

void PostProcessRenderer::setBloomEnabled(const bool& bloomEnabled) {
    if (m_postProcessUniformData.bloomEnabled != bloomEnabled) {
        m_postProcessUniformData.bloomEnabled = bloomEnabled;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->postProcessUniformDataChanged = true;
    }
}

float PostProcessRenderer::getBloomIntensity() const {
    return m_postProcessUniformData.bloomIntensity;
}

void PostProcessRenderer::setBloomIntensity(const float& bloomIntensity) {
    if (glm::notEqual(m_postProcessUniformData.bloomIntensity, bloomIntensity, 1e-5F)) {
        m_postProcessUniformData.bloomIntensity = bloomIntensity;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->postProcessUniformDataChanged = true;
    }
}

float PostProcessRenderer::getBloomBlurFilterRadius() const {
    return m_bloomBlurUniformData.filterRadius;
}

void PostProcessRenderer::setBloomBlurFilterRadius(const float& bloomBlurFilterRadius) {
    if (glm::notEqual(m_bloomBlurUniformData.filterRadius, bloomBlurFilterRadius, 1e-5F)) {
        m_bloomBlurUniformData.filterRadius = bloomBlurFilterRadius;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->bloomBlurUniformDataChanged = true;
    }
}

float PostProcessRenderer::getBloomThreshold() const {
    return m_bloomBlurUniformData.threshold;
}

void PostProcessRenderer::setBloomThreshold(const float& bloomThreshold) {
    if (glm::notEqual(m_bloomBlurUniformData.threshold, bloomThreshold, 1e-5F)) {
        m_bloomBlurUniformData.threshold = bloomThreshold;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->bloomBlurUniformDataChanged = true;
    }
}

float PostProcessRenderer::getBloomSoftThreshold() const {
    return m_bloomBlurUniformData.softThreshold;
}

void PostProcessRenderer::setBloomSoftThreshold(const float& bloomSoftThreshold) {
    if (glm::notEqual(m_bloomBlurUniformData.softThreshold, bloomSoftThreshold, 1e-5F)) {
        m_bloomBlurUniformData.softThreshold = bloomSoftThreshold;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->bloomBlurUniformDataChanged = true;
    }
}

uint32_t PostProcessRenderer::getMaxBloomBlurIterations() const {
    return m_bloomBlurMaxIterations;
}

uint32_t PostProcessRenderer::getBloomBlurIterations() const {
    return m_bloomBlurIterations;
}

void PostProcessRenderer::setBloomBlurIterations(const uint32_t& bloomBlurIterations) {
    m_bloomBlurIterations = glm::min(bloomBlurIterations, m_bloomBlurMaxIterations);
}

void PostProcessRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    bool success;
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->updateInputImage = true;
        success = createBloomBlurFramebuffer(m_resources[i]);
        assert(success);
    }
    success = createDownsampleGraphicsPipeline();
    assert(success);
    success = createUpsampleGraphicsPipeline();
    assert(success);
    success = createPostProcessGraphicsPipeline();
    assert(success);
}

bool PostProcessRenderer::createBloomBlurFramebuffer(RenderResources* resources) {
    for (auto& framebuffer : resources->bloomBlurMipFramebuffers)
        delete framebuffer;
    for (auto& imageView : resources->bloomBlurMipImageViews)
        delete imageView;
    delete resources->bloomBlurImage;
    delete resources->bloomTextureImageView;
    resources->bloomBlurIterations = glm::min(m_bloomBlurMaxIterations, m_bloomBlurIterations);

    resources->bloomBlurMipFramebuffers.clear();
    resources->bloomBlurMipImageViews.clear();

    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = sampleCount;
    imageConfig.mipLevels = m_resources->bloomBlurIterations;
    imageConfig.setSize(Engine::graphics()->getResolution());


    imageConfig.format = Engine::deferredLightingPass()->getOutputColourFormat();
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->bloomBlurImage = Image2D::create(imageConfig, "PostProcessRenderer-BloomBlurImage");

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();
    imageViewConfig.setImage(resources->bloomBlurImage);
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;

    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = Engine::graphics()->getDevice();

    glm::uvec2 resolution = Engine::graphics()->getResolution();
    ImageView* imageViews[1];
    for (uint32_t i = 0; i < m_resources->bloomBlurIterations; ++i) {
        assert(resolution.x > 0 && resolution.y > 0); // Resolution must not reach zero, we must ensure m_bloomBlurImageMipLevels is set to a valid value instead
        imageViewConfig.baseMipLevel = i;
        imageViewConfig.mipLevelCount = 1;
        ImageView* imageView = ImageView::create(imageViewConfig, "PostProcessRenderer-BloomBlurMipImageView");
        if (imageView == nullptr)
            return false;
        resources->bloomBlurMipImageViews.emplace_back(imageView);

        imageViews[0] = imageView;
        framebufferConfig.setSize(resolution);
        framebufferConfig.setAttachments(imageViews);
        framebufferConfig.setRenderPass(m_bloomBlurRenderPass.get());
        Framebuffer* framebuffer = Framebuffer::create(framebufferConfig, "PostProcessRenderer-BloomBlurFramebuffer");
        if (framebuffer == nullptr)
            return false;
        resources->bloomBlurMipFramebuffers.emplace_back(framebuffer);

        DescriptorSetWriter(resources->bloomBlurDescriptorSets[i])
                .writeImage(BLOOM_BLUR_SRC_TEXTURE_BINDING, m_frameSampler.get(), imageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();

        resolution /= 2;
    }

    imageViewConfig.baseMipLevel = 0;
    imageViewConfig.mipLevelCount = m_resources->bloomBlurIterations;
    resources->bloomTextureImageView = ImageView::create(imageViewConfig, "PostProcessRenderer-BloomTextureImageView");

    DescriptorSetWriter(resources->postProcessDescriptorSet)
            .writeImage(POSTPROCESS_BLOOM_TEXTURE_BINDING, m_frameSampler.get(), resources->bloomTextureImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
            .write();

    return true;
}

bool PostProcessRenderer::createDownsampleGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_bloomBlurRenderPass;
    pipelineConfig.setDynamicState(vk::DynamicState::eViewport, true);
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
//    pipelineConfig.depthTestEnabled = false;
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/postprocess/bloomBlur.frag";
    pipelineConfig.fragmentShaderEntryPoint = "downsampleStage";
    pipelineConfig.addDescriptorSetLayout(m_bloomBlurDescriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomBlurPushConstantData));
    pipelineConfig.setAttachmentBlendEnabled(0, false);
    return m_downsampleGraphicsPipeline->recreate(pipelineConfig, "PostProcessRenderer-DownsampleGraphicsPipeline");
}

bool PostProcessRenderer::createUpsampleGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_bloomBlurRenderPass;
    pipelineConfig.setDynamicState(vk::DynamicState::eViewport, true);
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
//    pipelineConfig.depthTestEnabled = false;
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/postprocess/bloomBlur.frag";
    pipelineConfig.fragmentShaderEntryPoint = "upsampleStage";
    pipelineConfig.addDescriptorSetLayout(m_bloomBlurDescriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomBlurPushConstantData));
    pipelineConfig.setAttachmentBlendEnabled(0, false);
    pipelineConfig.setAttachmentColourBlendMode(0, vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd);
    return m_upsampleGraphicsPipeline->recreate(pipelineConfig, "PostProcessRenderer-UpsampleGraphicsPipeline");
}

bool PostProcessRenderer::createPostProcessGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = Engine::graphics()->renderPass();
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/postprocess/postprocess.frag";
    pipelineConfig.addDescriptorSetLayout(m_postProcessDescriptorSetLayout.get());
    return m_postProcessGraphicsPipeline->recreate(pipelineConfig, "PostProcessRenderer-PostProcessGraphicsPipeline");
}

bool PostProcessRenderer::createBloomBlurRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, 1> attachments;

    attachments[0].setFormat(Engine::deferredLightingPass()->getOutputColourFormat());
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
//    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));

    m_bloomBlurRenderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig, "PostProcessRenderer-BloomBlurRenderPass"));
    return (bool)m_bloomBlurRenderPass;
}