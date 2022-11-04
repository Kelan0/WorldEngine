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

HistogramRenderer::HistogramRenderer():
        m_readbackBuffer(nullptr),
        m_histogramClearComputePipeline(nullptr),
        m_histogramAccumulationComputePipeline(nullptr),
        m_histogramAverageComputePipeline(nullptr),
        m_readbackNextFrame(false),
        m_resolution(100, 100) {
    m_resources.initDefault();
    setDownsampleFactor(2);
    setBinCount(256);
    setOffset(0.5F);
    setScale(0.06F);
    setMinBrightness(0.0F);
    setMaxBrightness(16.0F);
}

HistogramRenderer::~HistogramRenderer() {
    Engine::eventDispatcher()->disconnect(&HistogramRenderer::recreateSwapchain, this);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->descriptorSet;
        delete m_resources[i]->histogramBuffer;
    }

    delete m_histogramClearComputePipeline;
    delete m_histogramAccumulationComputePipeline;
    delete m_histogramAverageComputePipeline;

    delete m_readbackBuffer;
}

bool HistogramRenderer::init() {
    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addCombinedImageSampler(HISTOGRAM_INPUT_TEXTURE_BINDING, vk::ShaderStageFlagBits::eCompute)
            .addStorageBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, vk::ShaderStageFlagBits::eCompute)
            .build("HistogramRenderer-ComputeDescriptorSetLayout");

    m_histogramClearComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramClearComputePipeline");
    m_histogramAccumulationComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramAccumulationComputePipeline");
    m_histogramAverageComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "HistogramRenderer-HistogramAverageComputePipeline");

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

    Engine::eventDispatcher()->connect(&HistogramRenderer::recreateSwapchain, this);
    return true;
}

void HistogramRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("HistogramRenderer::render");
    PROFILE_BEGIN_GPU_CMD("HistogramRenderer::render", commandBuffer);

    m_resolution = glm::uvec2(Engine::graphics()->getResolution()) >> m_downsampleFactor;

    if (m_resources->frameTextureChanged) {
        m_resources->frameTextureChanged = false;
        ImageView* lightingFrameImageView = Engine::deferredRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(HISTOGRAM_INPUT_TEXTURE_BINDING, m_inputFrameSampler.get(), lightingFrameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    renderComputeHistogram(commandBuffer);


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
    m_binCount = glm::clamp(binCount, (uint32_t)32, (uint32_t)8192);
}

uint32_t HistogramRenderer::getDownsampleFactor() const {
    return m_downsampleFactor;
}

void HistogramRenderer::setDownsampleFactor(const uint32_t& downsampleFactor) {
    m_downsampleFactor = glm::min(downsampleFactor, (uint32_t)8);
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

Buffer* HistogramRenderer::getHistogramBuffer() const {
    return m_resources->histogramBuffer;
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

    m_histogramAverageComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& averagePipelineLayout = m_histogramAverageComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, clearPipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(clearPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData), &pushConstantData);
    // All work is performed within a single WorkGroup, TODO: ensure bin count is less than the minimum guaranteed WorkGroup size
    m_histogramAverageComputePipeline->dispatch(commandBuffer, 1, 1, 1);
}

void HistogramRenderer::readback(const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("HistogramRenderer::readback")
    PROFILE_BEGIN_GPU_CMD("HistogramRenderer::readback", commandBuffer)

    const size_t headerSize = sizeof(HistogramStorageBufferHeader);
    const size_t dataSize = sizeof(uint32_t) * m_binCount;

    if (m_readbackBuffer == nullptr || m_readbackBuffer->getSize() < headerSize + dataSize) {
        delete m_readbackBuffer;

        printf("Creating HistogramRenderer read-back buffer for %u bins\n", m_binCount);

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = headerSize + dataSize;
        bufferConfig.usage = vk::BufferUsageFlagBits::eTransferDst;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        m_readbackBuffer = Buffer::create(bufferConfig, "HistogramRenderer-ReadbackBuffer");
    }

    Buffer::copy(m_resources->histogramBuffer, m_readbackBuffer, headerSize + dataSize);

    PROFILE_END_GPU_CMD(commandBuffer)

    uint8_t* mappedDataPtr = m_readbackBuffer->map<uint8_t>();

    memcpy(&m_readbackHeader, mappedDataPtr, headerSize);

    m_readbackData.clear();
    m_readbackData.resize(m_binCount);
    memcpy(m_readbackData.data(), mappedDataPtr + headerSize, dataSize);
}

void HistogramRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    bool success;
    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->frameTextureChanged = true;
    }

    success = createHistogramClearComputePipeline();
    assert(success);

    success = createHistogramAccumulationComputePipeline();
    assert(success);

    success = createHistogramAverageComputePipeline();
    assert(success);
}

void HistogramRenderer::updateHistogramBuffer(RenderResources* resource) {
    size_t requiredSize = sizeof(HistogramStorageBufferHeader) + sizeof(uint32_t) * m_binCount;
    if (resource->histogramBuffer == nullptr || resource->histogramBuffer->getSize() != requiredSize) {
        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        bufferConfig.size = requiredSize;

        resource->histogramBuffer = Buffer::create(bufferConfig, "HistogramRenderer-HistogramBuffer");

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, resource->histogramBuffer->getBuffer())
                .write();
    }
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

bool HistogramRenderer::createHistogramAverageComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "res/shaders/histogram/histogram_average_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(HistogramPushConstantData));
    return m_histogramAverageComputePipeline->recreate(pipelineConfig, "HistogramRenderer-HistogramAverageComputePipeline");
}