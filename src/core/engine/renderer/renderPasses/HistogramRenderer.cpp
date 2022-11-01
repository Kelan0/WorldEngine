#include "core/engine/renderer/renderPasses/HistogramRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Texture.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/util/Profiler.h"

#define HISTOGRAM_INPUT_TEXTURE_BINDING 0

HistogramRenderer::HistogramRenderer():
        m_histogramGraphicsPipeline(nullptr),
        m_recreateVertexScatterMesh(true),
        m_resolution(100, 100) {
    m_resources.initDefault();
    setDownsampleFactor(2);
    setBinCount(256);
}

HistogramRenderer::~HistogramRenderer() {
    Engine::eventDispatcher()->disconnect(&HistogramRenderer::recreateSwapchain, this);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->descriptorSet;
        delete m_resources[i]->framebuffer;
        delete m_resources[i]->frameImageView;
        delete m_resources[i]->frameImage;
    }

    delete m_histogramGraphicsPipeline;
}

bool HistogramRenderer::init() {
    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addCombinedImageSampler(HISTOGRAM_INPUT_TEXTURE_BINDING, vk::ShaderStageFlagBits::eVertex)
            .build("HistogramRenderer-DescriptorSetLayout");

    m_histogramGraphicsPipeline = GraphicsPipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramGraphicsPipeline");

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.wrapV = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.minLod = 0.0F;
    samplerConfig.maxLod = 6.0F;
    m_sampler = Sampler::get(samplerConfig, "PostProcess-FrameSampler");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->descriptorSet = DescriptorSet::create(m_descriptorSetLayout, descriptorPool, "HistogramRenderer-DescriptorSet");
    }

    if (!createRenderPass()) {
        printf("Failed to create HistogramRenderer RenderPass\n");
        return false;
    }

    Engine::eventDispatcher()->connect(&HistogramRenderer::recreateSwapchain, this);
    return true;
}

void HistogramRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("HistogramRenderer::render");
    PROFILE_BEGIN_GPU_CMD("HistogramRenderer::render", commandBuffer);

    if (m_resources->recreateFramebuffer)
        createFramebuffer(m_resources.get());

    if (m_recreateVertexScatterMesh)
        createVertexScatterMesh();

    if (m_resources->vertexScatterMesh != m_vertexScatterMesh)
        m_resources->vertexScatterMesh = m_vertexScatterMesh; // shared_ptr will ensure the mesh being used by previous frames will remain allocated until that frame is done

    HistogramPushConstantData pushConstantData{};
    pushConstantData.resolution = m_resolution;
    pushConstantData.maxBrightness = 5.0F;
    pushConstantData.binCount = m_binCount;

    if (m_resources->frameTextureChanged) {
        m_resources->frameTextureChanged = false;
        ImageView* lightingFrameImageView = Engine::deferredRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(HISTOGRAM_INPUT_TEXTURE_BINDING, m_sampler.get(), lightingFrameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    Framebuffer* framebuffer = m_resources->framebuffer;
    m_renderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);

    m_histogramGraphicsPipeline->setViewport(commandBuffer, 0, framebuffer->getResolution());
    m_histogramGraphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    const vk::PipelineLayout& pipelineLayout = m_histogramGraphicsPipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);

    for (uint32_t i = 0; i < 4; ++i) {
        pushConstantData.channel = i;
        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(HistogramPushConstantData), &pushConstantData);
        m_resources->vertexScatterMesh->draw(commandBuffer, 1, 0);
    }

    commandBuffer.endRenderPass();

    PROFILE_END_GPU_CMD(commandBuffer);
}

uint32_t HistogramRenderer::getBinCount() const {
    return m_binCount;
}

void HistogramRenderer::setBinCount(const uint32_t& binCount) {
    uint32_t newBinCount = glm::clamp(binCount, (uint32_t)32, (uint32_t)8192);
    if (m_binCount != newBinCount) {
        m_binCount = newBinCount;
        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
            m_resources[i]->recreateFramebuffer = true;
    }
}

uint32_t HistogramRenderer::getDownsampleFactor() const {
    return m_downsampleFactor;
}

void HistogramRenderer::setDownsampleFactor(const uint32_t& downsampleFactor) {
    uint32_t newDownsampleFactor = glm::min(downsampleFactor, (uint32_t)8);
    if (m_downsampleFactor != newDownsampleFactor) {
        m_downsampleFactor = newDownsampleFactor;
        m_recreateVertexScatterMesh = true;
    }
}

ImageView* HistogramRenderer::getHistogramImageView() const {
    return m_resources->frameImageView;
}

const std::shared_ptr<Sampler>& HistogramRenderer::getSampler() const {
    return m_sampler;
}

void HistogramRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    bool success;
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->frameTextureChanged = true;
        success = createFramebuffer(m_resources[i]);
        assert(success);
    }
    success = createGraphicsPipeline();
    assert(success);

    m_recreateVertexScatterMesh = true;
}

bool HistogramRenderer::createFramebuffer(RenderResources* resource) {
    delete resource->framebuffer;
    delete resource->frameImageView;
    delete resource->frameImage;
    resource->recreateFramebuffer = false;

    // Histogram bin image (binCount x 1 pixels)
    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = vk::SampleCountFlagBits::e1;
    imageConfig.setSize(m_binCount, 1);
    imageConfig.format = Engine::deferredRenderer()->getOutputColourFormat();
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resource->frameImage = Image2D::create(imageConfig, "HistogramRenderer-FrameImage");

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resource->frameImage);
    resource->frameImageView = ImageView::create(imageViewConfig, "HistogramRenderer-FrameImageView");

    // Framebuffer
    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = Engine::graphics()->getDevice();
    framebufferConfig.setSize(m_binCount, 1);
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.addAttachment(resource->frameImageView);

    resource->framebuffer = Framebuffer::create(framebufferConfig, "HistogramRenderer-Framebuffer");
    if (resource->framebuffer == nullptr)
        return false;
    return true;
}

bool HistogramRenderer::createGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.setDynamicState(vk::DynamicState::eViewport, true);
    pipelineConfig.depthTestEnabled = false;
    pipelineConfig.vertexShader = "res/shaders/histogram/histogram.vert";
    pipelineConfig.fragmentShader = "res/shaders/histogram/histogram.frag";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addVertexInputBinding(0, sizeof(glm::vec2), vk::VertexInputRate::eVertex);
    pipelineConfig.addVertexInputAttribute(0, 0, vk::Format::eR32G32Sfloat, 0);
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(HistogramPushConstantData));
    pipelineConfig.setAttachmentBlendEnabled(0, true);
    pipelineConfig.setAttachmentColourBlendMode(0, vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd);
    pipelineConfig.primitiveTopology = vk::PrimitiveTopology::ePointList;
    return m_histogramGraphicsPipeline->recreate(pipelineConfig, "HistogramRenderer-HistogramGraphicsPipeline");
}

bool HistogramRenderer::createRenderPass() {
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
//    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));

    m_renderPass = SharedResource<RenderPass>(RenderPass::create(renderPassConfig, "HistogramRenderer-RenderPass"));
    return (bool)m_renderPass;
}

void HistogramRenderer::createVertexScatterMesh() {
    m_recreateVertexScatterMesh = false;

    glm::uvec2 resolution = Engine::graphics()->getResolution() / (1 << m_downsampleFactor);

    if (m_resolution != resolution) {
        m_resolution = resolution;

        // Create one vertex for every pixel [0.0 to 1.0]
        std::vector<glm::vec2> vertices;
        vertices.resize(resolution.x * resolution.y);

        printf("Creating HistogramRenderer vertex scatter mesh for resolution [%u x %u] (%llu vertices)\n", resolution.x, resolution.y, vertices.size());

        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].x = (float)(i / resolution.y) / (float)resolution.x;
            vertices[i].y = (float)(i % resolution.y) / (float)resolution.y;
        }
        MeshConfiguration meshConfig{};
        meshConfig.device = Engine::graphics()->getDevice();
        m_vertexScatterMesh = std::shared_ptr<Mesh>(Mesh::create(meshConfig, "HistogramRenderer-VertexScatterMesh"));
        m_vertexScatterMesh->uploadVertices(vertices.data(), sizeof(glm::vec2), vertices.size());
    }
}