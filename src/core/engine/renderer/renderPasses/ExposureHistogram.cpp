#include "core/engine/renderer/renderPasses/ExposureHistogram.h"
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
#define HISTOGRAM_PREV_OUTPUT_BUFFER_BINDING 2

struct ClearHistogramPushConstantData {
    uint32_t binCount;
    float offset;
    float scale;
};
struct AccumulateHistogramPushConstantData {
    glm::uvec2 resolution;
    uint32_t binCount;
    float offset;
    float scale;
};
struct AverageHistogramPushConstantData {
    uint32_t binCount;
    float offset;
    float scale;
    float lowPercent;
    float highPercent;
    float speedUp;
    float speedDown;
    float exposureCompensation;
    float dt;
};

ExposureHistogram::ExposureHistogram():
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
    setLowPercent(0.1F);
    setHighPercent(0.9F);
    setExposureSpeedUp(3.0F);
    setExposureSpeedDown(1.0F);
    setExposureCompensation(0.0F);
}

ExposureHistogram::~ExposureHistogram() {
    Engine::eventDispatcher()->disconnect(&ExposureHistogram::recreateSwapchain, this);

    for (uint32_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->descriptorSet;
        delete m_resources[i]->histogramBuffer;
    }

    delete m_histogramClearComputePipeline;
    delete m_histogramAccumulationComputePipeline;
    delete m_histogramAverageComputePipeline;

    delete m_readbackBuffer;
}

bool ExposureHistogram::init() {
    const SharedResource<DescriptorPool>& descriptorPool = Engine::graphics()->descriptorPool();

    m_descriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addCombinedImageSampler(HISTOGRAM_INPUT_TEXTURE_BINDING, vk::ShaderStageFlagBits::eCompute)
            .addStorageBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, vk::ShaderStageFlagBits::eCompute)
            .addStorageBuffer(HISTOGRAM_PREV_OUTPUT_BUFFER_BINDING, vk::ShaderStageFlagBits::eCompute)
            .build("ExposureHistogram-ComputeDescriptorSetLayout");

    m_histogramClearComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "ExposureHistogram-HistogramClearComputePipeline");
    m_histogramAccumulationComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "ExposureHistogram-HistogramAccumulationComputePipeline");
    m_histogramAverageComputePipeline = ComputePipeline::create(Engine::graphics()->getDevice(), "ExposureHistogram-HistogramAverageComputePipeline");

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
        m_resources[i]->descriptorSet = DescriptorSet::create(m_descriptorSetLayout, descriptorPool, "ExposureHistogram-DescriptorSet");
    }

    Engine::eventDispatcher()->connect(&ExposureHistogram::recreateSwapchain, this);
    return true;
}

void ExposureHistogram::update(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("ExposureHistogram::update");
    PROFILE_BEGIN_GPU_CMD("ExposureHistogram::update", commandBuffer);

    m_resolution = glm::uvec2(Engine::graphics()->getResolution()) >> m_downsampleFactor;

    if (m_resources->frameTextureChanged) {
        m_resources->frameTextureChanged = false;
        ImageView* lightingFrameImageView = Engine::deferredRenderer()->getOutputFrameImageView();

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeImage(HISTOGRAM_INPUT_TEXTURE_BINDING, m_inputFrameSampler.get(), lightingFrameImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
                .write();
    }

    if (updateHistogramBuffer(m_resources.get())) {
        m_resources[Engine::graphics()->getNextFrameIndex()]->prevHistogramBufferChanged = true;
    }

    if (m_resources->prevHistogramBufferChanged) {
        m_resources->prevHistogramBufferChanged = false;
        Buffer* prevHistogramBuffer = m_resources[Engine::graphics()->getPreviousFrameIndex()]->histogramBuffer;
        if (prevHistogramBuffer == nullptr)
            prevHistogramBuffer = m_resources->histogramBuffer;
        DescriptorSetWriter(m_resources->descriptorSet)
                .writeBuffer(HISTOGRAM_PREV_OUTPUT_BUFFER_BINDING, prevHistogramBuffer->getBuffer())
                .write();
    }

    ClearHistogramPushConstantData clearPushConstantData{};
    clearPushConstantData.binCount = m_binCount;
    clearPushConstantData.offset = m_offset;
    clearPushConstantData.scale = m_scale;

    AccumulateHistogramPushConstantData accumulatePushConstantData{};
    accumulatePushConstantData.resolution = m_resolution;
    accumulatePushConstantData.binCount = m_binCount;
    accumulatePushConstantData.offset = m_offset;
    accumulatePushConstantData.scale = m_scale;

    AverageHistogramPushConstantData averagePushConstantData{};
    averagePushConstantData.binCount = m_binCount;
    averagePushConstantData.offset = m_offset;
    averagePushConstantData.scale = m_scale;
    averagePushConstantData.lowPercent = m_lowPercent;
    averagePushConstantData.highPercent = m_highPercent;
    averagePushConstantData.speedUp = m_exposureSpeedUp;
    averagePushConstantData.speedDown = m_exposureSpeedDown;
    averagePushConstantData.exposureCompensation = m_exposureCompensation;
    averagePushConstantData.dt = (float)dt;


    std::array<vk::DescriptorSet, 1> descriptorSets = {
            m_resources->descriptorSet->getDescriptorSet()
    };

    constexpr uint32_t workgroupSize = 16;
    uint32_t workgroupCountX, workgroupCountY;

    m_histogramClearComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& clearPipelineLayout = m_histogramClearComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, clearPipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(clearPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ClearHistogramPushConstantData), &clearPushConstantData);
    workgroupCountX = INT_DIV_CEIL(m_binCount, workgroupSize);
    m_histogramClearComputePipeline->dispatch(commandBuffer, workgroupCountX, 1, 1);

    m_histogramAccumulationComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& accumulatePipelineLayout = m_histogramAccumulationComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, accumulatePipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(accumulatePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(AccumulateHistogramPushConstantData), &accumulatePushConstantData);

    workgroupCountX = INT_DIV_CEIL(m_resolution.x, workgroupSize);
    workgroupCountY = INT_DIV_CEIL(m_resolution.y, workgroupSize);
    m_histogramAccumulationComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);

    m_histogramAverageComputePipeline->bind(commandBuffer);
    const vk::PipelineLayout& averagePipelineLayout = m_histogramAverageComputePipeline->getPipelineLayout();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, averagePipelineLayout, 0, descriptorSets, nullptr);
    commandBuffer.pushConstants(averagePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(AverageHistogramPushConstantData), &averagePushConstantData);
    // All work is performed within a single WorkGroup, TODO: ensure bin count is less than the minimum guaranteed WorkGroup size
    m_histogramAverageComputePipeline->dispatch(commandBuffer, 1, 1, 1);

    m_readbackData.clear();
    if (m_readbackNextFrame) {
        m_readbackNextFrame = false;
        readback(commandBuffer);
    }

    PROFILE_END_GPU_CMD(commandBuffer);
}

uint32_t ExposureHistogram::getBinCount() const {
    return m_binCount;
}

void ExposureHistogram::setBinCount(const uint32_t& binCount) {
    m_binCount = glm::clamp(binCount, (uint32_t)32, (uint32_t)8192);
}

uint32_t ExposureHistogram::getDownsampleFactor() const {
    return m_downsampleFactor;
}

void ExposureHistogram::setDownsampleFactor(const uint32_t& downsampleFactor) {
    m_downsampleFactor = glm::min(downsampleFactor, (uint32_t)8);
}

float ExposureHistogram::getOffset() const {
    return m_offset;
}

void ExposureHistogram::setOffset(const float& offset) {
    m_offset = offset;
}

float ExposureHistogram::getScale() const {
    return m_scale;
}

void ExposureHistogram::setScale(const float& scale) {
    m_scale = scale;
}

float ExposureHistogram::getLowPercent() const {
    return m_lowPercent;
}

void ExposureHistogram::setLowPercent(const float& lowPercent) {
    m_lowPercent = lowPercent;
}

float ExposureHistogram::getHighPercent() const {
    return m_highPercent;
}

void ExposureHistogram::setHighPercent(const float& highPercent) {
    m_highPercent = highPercent;
}

float ExposureHistogram::getExposureSpeedUp() const {
    return m_exposureSpeedUp;
}

void ExposureHistogram::setExposureSpeedUp(const float& exposureSpeedUp) {
    m_exposureSpeedUp = exposureSpeedUp;
}

float ExposureHistogram::getExposureSpeedDown() const {
    return m_exposureSpeedDown;
}

void ExposureHistogram::setExposureSpeedDown(const float& exposureSpeedDown) {
    m_exposureSpeedDown = exposureSpeedDown;
}

float ExposureHistogram::getExposureCompensation() const {
    return m_exposureCompensation;
}

void ExposureHistogram::setExposureCompensation(const float& exposureCompensation) {
    m_exposureCompensation = exposureCompensation;
}

Buffer* ExposureHistogram::getHistogramBuffer() const {
    return m_resources->histogramBuffer;
}

void ExposureHistogram::readback(const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("ExposureHistogram::readback")
    PROFILE_BEGIN_GPU_CMD("ExposureHistogram::readback", commandBuffer)

    const size_t headerSize = sizeof(HistogramStorageBufferHeader);
    const size_t dataSize = sizeof(uint32_t) * m_binCount;

    if (m_readbackBuffer == nullptr || m_readbackBuffer->getSize() < headerSize + dataSize) {
        delete m_readbackBuffer;

        printf("Creating ExposureHistogram read-back buffer for %u bins\n", m_binCount);

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = headerSize + dataSize;
        bufferConfig.usage = vk::BufferUsageFlagBits::eTransferDst;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible;
        m_readbackBuffer = Buffer::create(bufferConfig, "ExposureHistogram-ReadbackBuffer");
    }

    Buffer::copy(m_resources->histogramBuffer, m_readbackBuffer, headerSize + dataSize);

    PROFILE_END_GPU_CMD(commandBuffer)

    uint8_t* mappedDataPtr = m_readbackBuffer->map<uint8_t>();

    memcpy(&m_readbackHeader, mappedDataPtr, headerSize);

    m_readbackData.clear();
    m_readbackData.resize(m_binCount);
    memcpy(m_readbackData.data(), mappedDataPtr + headerSize, dataSize);
}

void ExposureHistogram::recreateSwapchain(RecreateSwapchainEvent* event) {
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

bool ExposureHistogram::updateHistogramBuffer(RenderResources* resource) {
    size_t requiredSize = sizeof(HistogramStorageBufferHeader) + sizeof(uint32_t) * m_binCount;
    if (resource->histogramBuffer == nullptr || resource->histogramBuffer->getSize() != requiredSize) {
        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        bufferConfig.size = requiredSize;

        resource->histogramBuffer = Buffer::create(bufferConfig, "ExposureHistogram-HistogramBuffer");

        DescriptorSetWriter(m_resources->descriptorSet)
                .writeBuffer(HISTOGRAM_OUTPUT_BUFFER_BINDING, resource->histogramBuffer->getBuffer())
                .write();

        return true;
    }
    return false;
}

bool ExposureHistogram::createHistogramClearComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "shaders/histogram/histogram_clear_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(ClearHistogramPushConstantData));
    return m_histogramClearComputePipeline->recreate(pipelineConfig, "ExposureHistogram-HistogramClearComputePipeline");
}

bool ExposureHistogram::createHistogramAccumulationComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "shaders/histogram/histogram_accumulate_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(AccumulateHistogramPushConstantData));
    return m_histogramAccumulationComputePipeline->recreate(pipelineConfig, "ExposureHistogram-HistogramAccumulationComputePipeline");
}

bool ExposureHistogram::createHistogramAverageComputePipeline() {
    ComputePipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.computeShader = "shaders/histogram/histogram_average_compute.glsl";
    pipelineConfig.addDescriptorSetLayout(m_descriptorSetLayout.get());
    pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(AverageHistogramPushConstantData));
    return m_histogramAverageComputePipeline->recreate(pipelineConfig, "ExposureHistogram-HistogramAverageComputePipeline");
}