#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/engine/geometry/MeshData.h"
#include "core/application/Application.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Image.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Buffer.h"

#define UNIFORM_BUFFER_BINDING 0
#define ALBEDO_TEXTURE_BINDING 1
#define NORMAL_TEXTURE_BINDING 2
#define DEPTH_TEXTURE_BINDING 3

DeferredGeometryRenderPass::DeferredGeometryRenderPass() {

}

DeferredGeometryRenderPass::~DeferredGeometryRenderPass() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        for (size_t j = 0; j < NumAttachments; ++j) {
            delete m_resources[i]->imageViews[j];
            delete m_resources[i]->images[j];
        }
        delete m_resources[i]->framebuffer;
    }
    Application::instance()->eventDispatcher()->disconnect(&DeferredGeometryRenderPass::recreateSwapchain, this);
}

bool DeferredGeometryRenderPass::init() {
    m_graphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Application::instance()->graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Application::instance()->graphics()->descriptorPool();
    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->imageViews = {}; // Default initialize elements to nullptr
        m_resources[i]->images = {};
        m_resources[i]->framebuffer = nullptr;
    }

    if (!createRenderPass()) {
        printf("Failed to create DeferredRenderer RenderPass\n");
        return false;
    }

    Application::instance()->eventDispatcher()->connect(&DeferredGeometryRenderPass::recreateSwapchain, this);
    return true;
}

void DeferredGeometryRenderPass::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    m_renderPass->begin(commandBuffer, m_resources->framebuffer, subpassContents);
}

std::shared_ptr<RenderPass> DeferredGeometryRenderPass::getRenderPass() const {
    return m_renderPass;
}

GraphicsPipeline* DeferredGeometryRenderPass::getGraphicsPipeline() const {
    return m_graphicsPipeline.get();
}

ImageView2D* DeferredGeometryRenderPass::getAlbedoImageView() const {
    return m_resources->imageViews[Attachment_Albedo];
}

ImageView2D* DeferredGeometryRenderPass::getNormalImageView() const {
    return m_resources->imageViews[Attachment_Normal];
}

ImageView2D* DeferredGeometryRenderPass::getDepthImageView() const {
    return m_resources->imageViews[Attachment_Depth];
}

vk::Format DeferredGeometryRenderPass::getAttachmentFormat(const uint32_t& attachment) const {
    switch (attachment) {
        case Attachment_Albedo: return vk::Format::eR8G8B8A8Unorm;
        case Attachment_Normal: return vk::Format::eR16G16B16A16Sfloat;
        case Attachment_Depth: return Application::instance()->graphics()->getDepthFormat();
        default:
            assert(false);
            return vk::Format::eUndefined;
    }
}

void DeferredGeometryRenderPass::setResolution(const glm::uvec2& resolution) {
    m_resolution = resolution;
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i)
        createFramebuffer(m_resources[i]);
}

void DeferredGeometryRenderPass::setResolution(const vk::Extent2D& resolution) {
    setResolution(resolution.width, resolution.height);
}

void DeferredGeometryRenderPass::setResolution(const uint32_t& width, const uint32_t& height) {
    setResolution(glm::uvec2(width, height));
}

void DeferredGeometryRenderPass::recreateSwapchain(const RecreateSwapchainEvent& event) {
    setResolution(Application::instance()->graphics()->getResolution());
    createGraphicsPipeline();
}

bool DeferredGeometryRenderPass::createFramebuffer(RenderResources* resources) {
    for (const auto &imageView: resources->imageViews)
        delete imageView;
    for (const auto &image: resources->images)
        delete image;
    delete resources->framebuffer;

    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

    Image2DConfiguration imageConfig;
    imageConfig.device = Application::instance()->graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = sampleCount;
    imageConfig.setSize(m_resolution);

    ImageView2DConfiguration imageViewConfig;
    imageViewConfig.device = Application::instance()->graphics()->getDevice();

    // Albedo image
    imageConfig.format = getAttachmentFormat(Attachment_Albedo);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->images[Attachment_Albedo] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_Albedo);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resources->images[Attachment_Albedo]);
    resources->imageViews[Attachment_Albedo] = ImageView2D::create(imageViewConfig);

    // Normal image
    imageConfig.format = getAttachmentFormat(Attachment_Normal);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->images[Attachment_Normal] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_Normal);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resources->images[Attachment_Normal]);
    resources->imageViews[Attachment_Normal] = ImageView2D::create(imageViewConfig);

    // Depth image
    imageConfig.format = getAttachmentFormat(Attachment_Depth);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment;
    resources->images[Attachment_Depth] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_Depth);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
//    if (ImageUtil::isStencilAttachment(imageViewConfig.format))
//        imageViewConfig.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    imageViewConfig.setImage(resources->images[Attachment_Depth]);
    resources->imageViews[Attachment_Depth] = ImageView2D::create(imageViewConfig);

    // Framebuffer
    FramebufferConfiguration framebufferConfig;
    framebufferConfig.device = Application::instance()->graphics()->getDevice();
    framebufferConfig.setSize(m_resolution);
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.setAttachments(resources->imageViews);

    resources->framebuffer = Framebuffer::create(framebufferConfig);
    if (resources->framebuffer == nullptr)
        return false;
    return true;
}

bool DeferredGeometryRenderPass::createGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig;
    pipelineConfig.device = Application::instance()->graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.setViewport(m_resolution);
    pipelineConfig.vertexShader = "res/shaders/main.vert";
    pipelineConfig.fragmentShader = "res/shaders/main.frag";
    pipelineConfig.vertexInputBindings = MeshUtils::getVertexBindingDescriptions<Vertex>();
    pipelineConfig.vertexInputAttributes = MeshUtils::getVertexAttributeDescriptions<Vertex>();
    pipelineConfig.setAttachmentBlendState(0, AttachmentBlendState(false, 0b1111));
    pipelineConfig.setAttachmentBlendState(1, AttachmentBlendState(false, 0b1111));
    Application::instance()->sceneRenderer()->initPipelineDescriptorSetLayouts(pipelineConfig);
    return m_graphicsPipeline->recreate(pipelineConfig);
}

bool DeferredGeometryRenderPass::createRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, NumAttachments> attachments;

    attachments[Attachment_Albedo].setFormat(getAttachmentFormat(Attachment_Albedo));
    attachments[Attachment_Albedo].setSamples(samples);
    attachments[Attachment_Albedo].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_Albedo].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_Albedo].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_Albedo].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_Albedo].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_Albedo].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_Normal].setFormat(getAttachmentFormat(Attachment_Normal));
    attachments[Attachment_Normal].setSamples(samples);
    attachments[Attachment_Normal].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_Normal].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_Normal].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_Normal].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_Normal].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_Normal].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_Depth].setFormat(getAttachmentFormat(Attachment_Depth));
    attachments[Attachment_Depth].setSamples(samples);
    attachments[Attachment_Depth].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[Attachment_Depth].setStoreOp(vk::AttachmentStoreOp::eStore); // could be eDontCare if we don't need to sample the depth buffer
    attachments[Attachment_Depth].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_Depth].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_Depth].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_Depth].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal); // eDepthStencilAttachmentOptimal if we don't need to sample the depth buffer

    SubpassConfiguration subpassConfiguration;
    subpassConfiguration.addColourAttachment(Attachment_Albedo, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.addColourAttachment(Attachment_Normal, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.setDepthStencilAttachment(Attachment_Depth, vk::ImageLayout::eDepthStencilAttachmentOptimal);

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

    RenderPassConfiguration renderPassConfig;
    renderPassConfig.device = Application::instance()->graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.setSubpassDependencies(dependencies);
    renderPassConfig.setClearColour(Attachment_Albedo, glm::vec4(0.0F, 0.25F, 0.5F, 1.0F));
    renderPassConfig.setClearColour(Attachment_Normal, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));
    renderPassConfig.setClearDepth(Attachment_Depth, 1.0F);
    renderPassConfig.setClearStencil(Attachment_Depth, 0);

    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig));
    return !!m_renderPass;
}









DeferredLightingRenderPass::DeferredLightingRenderPass(DeferredGeometryRenderPass* geometryPass):
    m_geometryPass(geometryPass) {
}

DeferredLightingRenderPass::~DeferredLightingRenderPass() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->uniformDescriptorSet;
        delete m_resources[i]->uniformBuffer;
    }
    for (size_t i = 0; i < NumAttachments; ++i)
        delete m_attachmentSamplers[i];

    Application::instance()->eventDispatcher()->disconnect(&DeferredLightingRenderPass::recreateSwapchain, this);
}

bool DeferredLightingRenderPass::init() {
    m_graphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Application::instance()->graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Application::instance()->graphics()->descriptorPool();

    m_uniformDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBlock(UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment, sizeof(LightingPassUniformData))
            .addCombinedImageSampler(ALBEDO_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(NORMAL_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(DEPTH_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build();

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());

        BufferConfiguration uniformBufferConfig;
        uniformBufferConfig.device = Application::instance()->graphics()->getDevice();
        uniformBufferConfig.size = sizeof(LightingPassUniformData);
        uniformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        uniformBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        m_resources[i]->uniformBuffer = Buffer::create(uniformBufferConfig);

        m_resources[i]->uniformDescriptorSet = DescriptorSet::create(m_uniformDescriptorSetLayout, descriptorPool);

        DescriptorSetWriter(m_resources[i]->uniformDescriptorSet)
                .writeBuffer(UNIFORM_BUFFER_BINDING, m_resources[i]->uniformBuffer, 0, m_resources[i]->uniformBuffer->getSize())
                .write();
    }

    SamplerConfiguration samplerConfig;
    samplerConfig.device = Application::instance()->graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eNearest;
    samplerConfig.magFilter = vk::Filter::eNearest;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;

    m_attachmentSamplers[Attachment_Albedo] = Sampler::create(samplerConfig);
    m_attachmentSamplers[Attachment_Normal] = Sampler::create(samplerConfig);
    m_attachmentSamplers[Attachment_Depth] = Sampler::create(samplerConfig);

    Application::instance()->eventDispatcher()->connect(&DeferredLightingRenderPass::recreateSwapchain, this);
    return true;
}

void DeferredLightingRenderPass::renderScreen(double dt) {
    const vk::CommandBuffer& commandBuffer = Application::instance()->graphics()->getCurrentCommandBuffer();
    const Framebuffer* framebuffer = Application::instance()->graphics()->getCurrentFramebuffer();
    const auto& renderPass = Application::instance()->graphics()->renderPass();
    renderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);

    m_graphicsPipeline->bind(commandBuffer);

    DescriptorSetWriter(m_resources->uniformDescriptorSet)
            .writeImage(ALBEDO_TEXTURE_BINDING, m_attachmentSamplers[Attachment_Albedo], m_geometryPass->getAlbedoImageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
            .writeImage(NORMAL_TEXTURE_BINDING, m_attachmentSamplers[Attachment_Normal], m_geometryPass->getNormalImageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
            .writeImage(DEPTH_TEXTURE_BINDING, m_attachmentSamplers[Attachment_Depth], m_geometryPass->getDepthImageView(), vk::ImageLayout::eShaderReadOnlyOptimal)
            .write();

    std::vector<vk::DescriptorSet> descriptorSets = {
            m_resources->uniformDescriptorSet->getDescriptorSet()
    };

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline->getPipelineLayout(), 0, descriptorSets, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();
}

GraphicsPipeline* DeferredLightingRenderPass::getGraphicsPipeline() const {
    return m_graphicsPipeline.get();
}

void DeferredLightingRenderPass::recreateSwapchain(const RecreateSwapchainEvent& event) {
    GraphicsPipelineConfiguration pipelineConfig;
    pipelineConfig.device = Application::instance()->graphics()->getDevice();
    pipelineConfig.renderPass = Application::instance()->graphics()->renderPass();
    pipelineConfig.setViewport(0, 0); // Default to full window resolution
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/deferred/lighting.frag";
//    pipelineConfig.addVertexInputBinding(0, sizeof(glm::vec2), vk::VertexInputRate::eVertex);
//    pipelineConfig.addVertexInputAttribute(0, 0, vk::Format::eR32G32Sfloat, 0); // XY
    pipelineConfig.addDescriptorSetLayout(m_uniformDescriptorSetLayout.get());
    m_graphicsPipeline->recreate(pipelineConfig);
}