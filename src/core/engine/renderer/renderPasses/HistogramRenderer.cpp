#include "core/engine/renderer/renderPasses/HistogramRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Texture.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/util/Profiler.h"
#include "extern/imgui/imgui.h"
#include "extern/imgui/implot.h"

#define HISTOGRAM_INPUT_TEXTURE_BINDING 0
#define HISTOGRAM_OUTPUT_BUFFER_BINDING 1

constexpr bool UseVertexScattering = false;

HistogramRenderer::HistogramRenderer():
        m_readbackBuffer(nullptr),
        m_histogramVertexScatterGraphicsPipeline(nullptr),
        m_histogramClearComputePipeline(nullptr),
        m_histogramAccumulationComputePipeline(nullptr),
        m_recreateVertexScatterMesh(true),
        m_readbackNextFrame(false),
        m_resolution(100, 100) {
    m_resources.initDefault();
    setDownsampleFactor(2);
    setBinCount(256);
    setOffset(0.5F);
    setScale(0.06F);
    setMinBrightness(0.0F);
    setMaxBrightness(16.0F);
    setEnabledChannels(glm::bvec4(true, true, true, true));
}

HistogramRenderer::~HistogramRenderer() {
    Engine::eventDispatcher()->disconnect(&HistogramRenderer::recreateSwapchain, this);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->descriptorSet;
        if (UseVertexScattering) {
            delete m_resources[i]->vertexScatterFramebuffer;
            delete m_resources[i]->histogramImageView;
            delete m_resources[i]->histogramImage;
        } else {
            delete m_resources[i]->histogramBuffer;
        }
    }

    if (UseVertexScattering) {
        delete m_histogramVertexScatterGraphicsPipeline;
    } else {
        delete m_histogramClearComputePipeline;
        delete m_histogramAccumulationComputePipeline;
    }

    delete m_readbackBuffer;
}

bool HistogramRenderer::init() {
    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    if (UseVertexScattering) {
        m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
                .addCombinedImageSampler(HISTOGRAM_INPUT_TEXTURE_BINDING, vk::ShaderStageFlagBits::eVertex)
                .build("HistogramRenderer-VertexScatterDescriptorSetLayout");
    } else {
        m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
                .addCombinedImageSampler(HISTOGRAM_INPUT_TEXTURE_BINDING, vk::ShaderStageFlagBits::eCompute)
                .addStorageBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, vk::ShaderStageFlagBits::eCompute)
                .build("HistogramRenderer-ComputeDescriptorSetLayout");
    }

    if (UseVertexScattering) {
        m_histogramVertexScatterGraphicsPipeline = GraphicsPipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramVertexScatterGraphicsPipeline");
    } else {
        m_histogramClearComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramClearComputePipeline");
        m_histogramAccumulationComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramAccumulationComputePipeline");
    }

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.wrapV = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.minLod = 0.0F;
    samplerConfig.maxLod = 6.0F;
    m_inputFrameSampler = Sampler::get(samplerConfig, "PostProcess-FrameSampler");

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->descriptorSet = DescriptorSet::create(m_descriptorSetLayout, descriptorPool, "HistogramRenderer-DescriptorSet");
    }

    if (UseVertexScattering) {
        if (!createVertexScatterRenderPass()) {
            printf("Failed to create HistogramRenderer RenderPass\n");
            return false;
        }
    }


    Engine::eventDispatcher()->connect(&HistogramRenderer::recreateSwapchain, this);
    return true;
}

void HistogramRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("HistogramRenderer::render");
    PROFILE_BEGIN_GPU_CMD("HistogramRenderer::render", commandBuffer);

    updateReadbackBuffer();

    if (m_resources->frameTextureChanged) {
        m_resources->frameTextureChanged = false;
        ImageView* lightingFrameImageView = Engine::deferredRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(HISTOGRAM_INPUT_TEXTURE_BINDING, m_inputFrameSampler.get(), lightingFrameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    if (UseVertexScattering) {
        renderVertexScatterHistogram(commandBuffer);
    } else {
        renderComputeHistogram(commandBuffer);
    }


    m_readbackData.clear();
    if (m_readbackNextFrame) {
        m_readbackNextFrame = false;
        readback(commandBuffer);
    }

    PROFILE_END_GPU_CMD(commandBuffer);
}

uint32_t HistogramRenderer::getBinCount() const {
    return m_binCount;
}

void HistogramRenderer::setBinCount(const uint32_t& binCount) {
    uint32_t newBinCount = glm::clamp(binCount, (uint32_t)32, (uint32_t)8192);
    if (m_binCount != newBinCount) {
        m_binCount = newBinCount;
//        for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i)
//            m_resources[i]->binCountChanged = true;
    }
}

uint32_t HistogramRenderer::getDownsampleFactor() const {
    return m_downsampleFactor;
}

void HistogramRenderer::setDownsampleFactor(const uint32_t& downsampleFactor) {
    uint32_t newDownsampleFactor = glm::min(downsampleFactor, (uint32_t)8);
    if (m_downsampleFactor != newDownsampleFactor) {
        m_downsampleFactor = newDownsampleFactor;
        if (UseVertexScattering) {
            m_recreateVertexScatterMesh = true;
        }
    }
}

float HistogramRenderer::getOffset() const {
    return m_offset;
}

void HistogramRenderer::setOffset(const float& offset) {
    m_offset = offset;
}

float HistogramRenderer::getScale() const {
    return m_scale;
}

void HistogramRenderer::setScale(const float& scale) {
    m_scale = scale;
}

float HistogramRenderer::getMinBrightness() const {
    return m_minBrightness;
}

void HistogramRenderer::setMinBrightness(const float& minBrightness) {
    m_minBrightness = minBrightness;
}

float HistogramRenderer::getMaxBrightness() const {
    return m_maxBrightness;
}

void HistogramRenderer::setMaxBrightness(const float& maxBrightness) {
    m_maxBrightness = maxBrightness;
}

glm::bvec4 HistogramRenderer::getEnabledChannels() const {
    return m_enabledChannels;
}

void HistogramRenderer::setEnabledChannels(const glm::bvec4& enabledChannels) {
    m_enabledChannels = enabledChannels;
}

Buffer* HistogramRenderer::getHistogramBuffer() const {
    return m_resources->histogramBuffer;
}

ImageView* HistogramRenderer::getHistogramImageView() const {
    return m_resources->histogramImageView;
}

const std::shared_ptr<Sampler>& HistogramRenderer::getSampler() const {
    return m_inputFrameSampler;
}

void HistogramRenderer::renderVertexScatterHistogram(const vk::CommandBuffer& commandBuffer) {
    if (m_resources->vertexScatterFramebuffer->getWidth() != m_binCount) {
        createVertexScatterFramebuffer(m_resources.get());
    }

    if (m_recreateVertexScatterMesh) {
        m_recreateVertexScatterMesh = false;
        createVertexScatterMesh();
    }

    if (m_resources->vertexScatterMesh != m_vertexScatterMesh) {
        m_resources->vertexScatterMesh = m_vertexScatterMesh; // shared_ptr will ensure the mesh being used by previous frames will remain allocated until that frame is done
    }

    HistogramPushConstantData pushConstantData{};
    pushConstantData.resolution = m_resolution;
    pushConstantData.maxBrightness = 5.0F;
    pushConstantData.binCount = m_binCount;
    pushConstantData.offset = m_offset;
    pushConstantData.scale = m_scale;

    Framebuffer* framebuffer = m_resources->vertexScatterFramebuffer;
    m_renderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);

    m_histogramVertexScatterGraphicsPipeline->setViewport(commandBuffer, 0, framebuffer->getResolution());
    m_histogramVertexScatterGraphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    const vk::PipelineLayout& pipelineLayout = m_histogramVertexScatterGraphicsPipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, descriptorSets, nullptr);

    for (uint32_t i = 0; i < 4; ++i) {
        if (m_enabledChannels[i]) {
            pushConstantData.channel = i; // TODO: one draw call for 4 instances, rather than 4 draw calls
            commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(HistogramPushConstantData), &pushConstantData);
            m_resources->vertexScatterMesh->draw(commandBuffer, 1, 0);
        }
    }

    commandBuffer.endRenderPass();
}

void HistogramRenderer::renderComputeHistogram(const vk::CommandBuffer& commandBuffer) {

    updateHistogramBuffer(m_resources.get());

    HistogramPushConstantData pushConstantData{};
    pushConstantData.resolution = m_resolution;
    pushConstantData.maxBrightness = 5.0F;
    pushConstantData.binCount = m_binCount;
    pushConstantData.offset = m_offset;
    pushConstantData.scale = m_scale;


    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    constexpr uint32_t workgroupSize = 16;
    uint32_t workgroupCountX, workgroupCountY;

    m_histogramClearComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& clearPipelineLayout = m_histogramAccumulationComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, clearPipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(clearPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData), &pushConstantData);
    workgroupCountX = INT_DIV_CEIL(m_binCount, workgroupSize);
    m_histogramClearComputePipeline->dispatch(commandBuffer, workgroupCountX, 1, 1);

    m_histogramAccumulationComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& accumulatePipelineLayout = m_histogramAccumulationComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, accumulatePipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(accumulatePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData), &pushConstantData);

    workgroupCountX = INT_DIV_CEIL(m_resolution.x, workgroupSize);
    workgroupCountY = INT_DIV_CEIL(m_resolution.y, workgroupSize);
    m_histogramAccumulationComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
}

void HistogramRenderer::readback(const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("HistogramRenderer::readback")
    PROFILE_BEGIN_GPU_CMD("HistogramRenderer::readback", commandBuffer)
    const vk::Image& srcImage = m_resources->histogramImage->getImage();
    const vk::Buffer& dstBuffer = m_readbackBuffer->getBuffer();

    ImageUtil::transitionLayout(commandBuffer, srcImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
                                ImageTransition::General(vk::PipelineStageFlagBits::eFragmentShader),
                                ImageTransition::General(vk::PipelineStageFlagBits::eFragmentShader));
//                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader),
//                                ImageTransition::TransferSrc());
    vk::BufferImageCopy imageCopy;
    imageCopy.bufferOffset = 0;
    imageCopy.bufferRowLength = 0;
    imageCopy.bufferImageHeight = 0;
    imageCopy.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    imageCopy.imageOffset = vk::Offset3D(0, 0, 0);
    imageCopy.imageExtent = vk::Extent3D(m_binCount, 1, 1);
    commandBuffer.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstBuffer, 1, &imageCopy);

//    ImageUtil::transitionLayout(commandBuffer, srcImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
//                                ImageTransition::TransferSrc(),
//                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader));

    PROFILE_END_GPU_CMD(commandBuffer)

    m_readbackData.clear();
    m_readbackData.resize(m_binCount);
    memcpy(m_readbackData.data(), m_readbackBuffer->map(), sizeof(glm::vec4) * m_binCount);
}

void HistogramRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    bool success;
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->frameTextureChanged = true;
        if (UseVertexScattering) {
            success = createVertexScatterFramebuffer(m_resources[i]);
            assert(success);
        }
    }
    if (UseVertexScattering) {
        m_recreateVertexScatterMesh = true;
        success = createVertexScatterGraphicsPipeline();
        assert(success);
    } else {
        success = createHistogramClearComputePipeline();
        assert(success);

        success = createHistogramAccumulationComputePipeline();
        assert(success);
    }
}

bool HistogramRenderer::createVertexScatterFramebuffer(RenderResources* resource) {
    delete resource->vertexScatterFramebuffer;
    delete resource->histogramImageView;
    delete resource->histogramImage;

//    resource->binCountChanged = false;

    // Histogram bin image (binCount x 1 pixels)
    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = vk::SampleCountFlagBits::e1;
    imageConfig.setSize(m_binCount, 1);
//    imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
    imageConfig.format = vk::Format::eR32Sfloat;
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
//    imageConfig.enabledTexelAccess = true; // Linear tiling
    resource->histogramImage = Image2D::create(imageConfig, "HistogramRenderer-HistogramImage");

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resource->histogramImage);
    resource->histogramImageView = ImageView::create(imageViewConfig, "HistogramRenderer-HistogramImageView");

    // Framebuffer
    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = Engine::graphics()->getDevice();
    framebufferConfig.setSize(m_binCount, 1);
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.addAttachment(resource->histogramImageView);

    resource->vertexScatterFramebuffer = Framebuffer::create(framebufferConfig, "HistogramRenderer-VertexScatterFramebuffer");
    if (resource->vertexScatterFramebuffer == nullptr)
        return false;
    return true;
}

void HistogramRenderer::updateHistogramBuffer(RenderResources* resource) {
    if (resource->histogramBuffer == nullptr || resource->histogramBuffer->getSize<uint32_t>() != m_binCount) {
        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        bufferConfig.size = sizeof(uint32_t) * m_binCount;

        resource->histogramBuffer = Buffer::create(bufferConfig, "HistogramRenderer-HistogramBuffer");

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, resource->histogramBuffer->getBuffer())
                .write();
    }
}

bool HistogramRenderer::createVertexScatterGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.setDynamicState(vk::DynamicState::eViewport, true);
    pipelineConfig.depthTestEnabled = false;
    pipelineConfig.vertexShader = "res/shaders/histogram/histogram_vtx_scatter.vert";
    pipelineConfig.fragmentShader = "res/shaders/histogram/histogram_vtx_scatter.frag";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addVertexInputBinding(0, sizeof(glm::vec2), vk::VertexInputRate::eVertex);
    pipelineConfig.addVertexInputAttribute(0, 0, vk::Format::eR32G32Sfloat, 0);
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(HistogramPushConstantData));
    pipelineConfig.setAttachmentBlendEnabled(0, true);
    pipelineConfig.setAttachmentColourBlendMode(0, vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd);
    pipelineConfig.primitiveTopology = vk::PrimitiveTopology::ePointList;
    return m_histogramVertexScatterGraphicsPipeline->recreate(pipelineConfig, "HistogramRenderer-VertexScatterGraphicsPipeline");
}

bool HistogramRenderer::createHistogramClearComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "res/shaders/histogram/histogram_clear_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData));
    return m_histogramClearComputePipeline->recreate(pipelineConfig, "HistogramRenderer-HistogramClearComputePipeline");
}

bool HistogramRenderer::createHistogramAccumulationComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "res/shaders/histogram/histogram_accumulate_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData));
    return m_histogramAccumulationComputePipeline->recreate(pipelineConfig, "HistogramRenderer-HistogramAccumulationComputePipeline");
}

bool HistogramRenderer::createVertexScatterRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, 1> attachments;

    attachments[0].setFormat(vk::Format::eR32Sfloat);
    attachments[0].setSamples(samples);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[0].setFinalLayout(vk::ImageLayout::eGeneral);

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

void HistogramRenderer::updateReadbackBuffer() {
    vk::DeviceSize requiredSize = sizeof(glm::vec4) * m_binCount;
    if (m_readbackBuffer == nullptr || m_readbackBuffer->getSize() < requiredSize) {
        delete m_readbackBuffer;

        printf("Creating HistogramRenderer read-back buffer for %u bins\n", m_binCount);

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = requiredSize;
        bufferConfig.usage = vk::BufferUsageFlagBits::eTransferDst;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        m_readbackBuffer = Buffer::create(bufferConfig, "HistogramRenderer-ReadbackBuffer");
    }
}