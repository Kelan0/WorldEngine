

#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/engine/scene/event/Events.h"
#include "core/application/Application.h"
#include "core/graphics/ImageCube.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/CommandPool.h"



struct DiffuseIrradianceComputeUBO {
    glm::uvec2 srcMapSize;
    glm::uvec2 dstMapSize;
};

ComputePipeline* EnvironmentMap::s_diffuseIrradianceConvolutionComputePipeline = nullptr;
DescriptorSet* EnvironmentMap::s_diffuseIrradianceConvolutionDescriptorSet = nullptr;
Buffer* EnvironmentMap::s_uniformBuffer = nullptr;



EnvironmentMap::EnvironmentMap(const uint32_t& irradianceMapSize, const uint32_t& specularMapSize):
    m_irradianceMapSize(irradianceMapSize),
    m_specularMapSize(specularMapSize),
    m_needsRecompute(false) {
}

EnvironmentMap::~EnvironmentMap() {
    m_specularReflectionMapTexture.reset();
    m_diffuseIrradianceMapTexture.reset();
    m_environmentMapTexture.reset();
    m_specularReflectionImage.reset();
    m_diffuseIrradianceImage.reset();
    m_environmentImage.reset();
}

void EnvironmentMap::update() {
    PROFILE_SCOPE("EnvironmentMap::update");

    if (m_needsRecompute) {
        m_needsRecompute = false;

        printf("Updating environment map\n");
        auto t0 = Performance::now();

        if (!m_environmentImage)
            return; // No environment image, do nothing

        if (!m_diffuseIrradianceImage) {

            ImageCubeConfiguration imageConfig;
            imageConfig.device = Application::instance()->graphics()->getDevice();
            imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageConfig.size = m_irradianceMapSize;
            imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
            imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
            imageConfig.mipLevels = 1;
            imageConfig.generateMipmap = false;
            m_diffuseIrradianceImage = std::shared_ptr<ImageCube>(ImageCube::create(imageConfig));
            if (m_diffuseIrradianceImage == nullptr) {
                printf("Failed to create diffuse irradiance image for environment map\n");
                assert(false);
                return;
            }

            m_diffuseIrradianceMapTexture = createTexture(m_diffuseIrradianceImage);
        }

        const std::shared_ptr<CommandPool>& commandPool = Application::instance()->graphics()->commandPool();
        const vk::CommandBuffer& commandBuffer = **commandPool->getOrCreateCommandBuffer("compute_main", { vk::CommandBufferLevel::ePrimary });
        const vk::Queue& computeQueue = **Application::instance()->graphics()->getQueue(QUEUE_COMPUTE_MAIN);

        vk::CommandBufferBeginInfo commandBeginInfo;
        commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer.begin(commandBeginInfo);

        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange.setBaseArrayLayer(0);
        subresourceRange.setLayerCount(6);
        subresourceRange.setBaseMipLevel(0);
        subresourceRange.setLevelCount(m_diffuseIrradianceImage->getMipLevelCount());
        subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);

        ImageUtil::transitionLayout(commandBuffer, m_diffuseIrradianceImage->getImage(), subresourceRange, ImageTransition::FromAny(), ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));

        calculateDiffuseIrradiance(commandBuffer);

        ImageUtil::transitionLayout(commandBuffer, m_diffuseIrradianceImage->getImage(), subresourceRange, ImageTransition::FromAny(), ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader));

        commandBuffer.end();

        vk::SubmitInfo queueSubmitInfo;
        queueSubmitInfo.setCommandBufferCount(1);
        queueSubmitInfo.setPCommandBuffers(&commandBuffer);
        computeQueue.submit(1, &queueSubmitInfo, VK_NULL_HANDLE);
        computeQueue.waitIdle();

        printf("Updating environment map took %.2f msec\n", Performance::milliseconds(t0));
    }
}

void EnvironmentMap::setEnvironmentImage(const std::shared_ptr<ImageCube>& environmentImage) {
    m_environmentImage = environmentImage;
    m_environmentMapTexture = createTexture(environmentImage);
    m_needsRecompute = true;
}

const std::shared_ptr<ImageCube>& EnvironmentMap::getEnvironmentImage() const {
    return m_environmentImage;
}

const std::shared_ptr<ImageCube>& EnvironmentMap::getDiffuseIrradianceImage() const {
    return m_diffuseIrradianceImage;
}

const std::shared_ptr<ImageCube>& EnvironmentMap::getSpecularReflectionImage() const {
    return m_specularReflectionImage;
}

const std::shared_ptr<Texture>& EnvironmentMap::getEnvironmentMapTexture() const {
    return m_environmentMapTexture;
}

const std::shared_ptr<Texture>& EnvironmentMap::getDiffuseIrradianceMapTexture() const {
    return m_diffuseIrradianceMapTexture;
}

const std::shared_ptr<Texture>& EnvironmentMap::getSpecularReflectionMapTexture() const {
    return m_specularReflectionMapTexture;
}

void EnvironmentMap::calculateDiffuseIrradiance(const vk::CommandBuffer& commandBuffer) const {
    PROFILE_SCOPE("EnvironmentMap::calculateDiffuseIrradiance");

    printf("Recomputing diffuse-irradiance environment map\n");

    ComputePipeline* computePipeline = getDiffuseIrradianceComputePipeline();
    DescriptorSet* descriptorSet = getDiffuseIrradianceComputeDescriptorSet();
    Buffer* uniformBuffer = getUniformBuffer(sizeof(DiffuseIrradianceComputeUBO));

    DiffuseIrradianceComputeUBO uniformData{};
    uniformData.srcMapSize.x = m_environmentImage->getWidth();
    uniformData.srcMapSize.y = m_environmentImage->getHeight();
    uniformData.dstMapSize.x = m_irradianceMapSize;
    uniformData.dstMapSize.y = m_irradianceMapSize;
    uniformBuffer->upload(0, sizeof(DiffuseIrradianceComputeUBO), &uniformData);

    DescriptorSetWriter(descriptorSet)
    .writeBuffer(0, uniformBuffer)
    .writeImage(1, m_environmentMapTexture.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
    .writeImage(2, m_diffuseIrradianceMapTexture.get(), vk::ImageLayout::eGeneral) // is eGeneral the only option here? Nothing for ShaderWriteOnly use case? :(
    .write();

    constexpr uint32_t workgroupSize = 1;

    std::vector<vk::DescriptorSet> descriptorSets = { descriptorSet->getDescriptorSet() };

    computePipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,computePipeline->getPipelineLayout(), 0, descriptorSets,nullptr);
    computePipeline->dispatch(commandBuffer, (uint32_t) glm::ceil(m_irradianceMapSize / workgroupSize),(uint32_t) glm::ceil(m_irradianceMapSize / workgroupSize), 6);

}

void EnvironmentMap::calculateSpecularReflection(const vk::CommandBuffer& commandBuffer) const {

}

DescriptorSet* EnvironmentMap::getDiffuseIrradianceComputeDescriptorSet() {
    if (s_diffuseIrradianceConvolutionDescriptorSet == nullptr) {
        std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(Application::instance()->graphics()->getDevice())
                .addUniformBuffer(0, vk::ShaderStageFlagBits::eCompute, sizeof(DiffuseIrradianceComputeUBO))
                .addCombinedImageSampler(1, vk::ShaderStageFlagBits::eCompute)
                .addStorageImage(2, vk::ShaderStageFlagBits::eCompute)
                .build();
        s_diffuseIrradianceConvolutionDescriptorSet = DescriptorSet::create(descriptorSetLayout,Application::instance()->graphics()->descriptorPool());
        assert(s_diffuseIrradianceConvolutionDescriptorSet != nullptr);

        Application::instance()->eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_diffuseIrradianceConvolutionDescriptorSet;
}

ComputePipeline* EnvironmentMap::getDiffuseIrradianceComputePipeline() {
    if (s_diffuseIrradianceConvolutionComputePipeline == nullptr) {
        ComputePipelineConfiguration pipelineConfig;
        pipelineConfig.device = Application::instance()->graphics()->getDevice();
        pipelineConfig.computeShader = "res/shaders/compute/compute_diffuseIrradiance.glsl";
        pipelineConfig.addDescriptorSetLayout(getDiffuseIrradianceComputeDescriptorSet()->getLayout().get());
        s_diffuseIrradianceConvolutionComputePipeline = ComputePipeline::create(pipelineConfig);
        assert(s_diffuseIrradianceConvolutionComputePipeline != nullptr);

        Application::instance()->eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_diffuseIrradianceConvolutionComputePipeline;
}

Buffer* EnvironmentMap::getUniformBuffer(const vk::DeviceSize& size) {
    if (s_uniformBuffer == nullptr || s_uniformBuffer->getSize() < size) {
        BufferConfiguration bufferConfig;
        bufferConfig.device = Application::instance()->graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bufferConfig.size = size;
        delete s_uniformBuffer;
        s_uniformBuffer = Buffer::create(bufferConfig);
        assert(s_uniformBuffer != nullptr);

        Application::instance()->eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_uniformBuffer;
}


std::shared_ptr<Texture> EnvironmentMap::createTexture(const std::shared_ptr<ImageCube>& image) const {
    if (image == nullptr)
        return nullptr;

    ImageViewConfiguration imageViewConfig;
    imageViewConfig.device = Application::instance()->graphics()->getDevice();
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(image.get());
    imageViewConfig.format = image->getFormat();
    imageViewConfig.baseArrayLayer = 0;
    imageViewConfig.arrayLayerCount = 6;
    imageViewConfig.baseMipLevel = 0;
    imageViewConfig.mipLevelCount = image->getMipLevelCount();

    SamplerConfiguration samplerConfig;
    samplerConfig.device = Application::instance()->graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    return std::shared_ptr<Texture>(Texture::create(imageViewConfig, samplerConfig));
}

void EnvironmentMap::onCleanupGraphics(const ShutdownGraphicsEvent& event) {
    delete s_uniformBuffer;
    delete s_diffuseIrradianceConvolutionDescriptorSet;
    delete s_diffuseIrradianceConvolutionComputePipeline;
}