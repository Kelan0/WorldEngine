

#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/engine/scene/event/Events.h"
#include "core/application/Application.h"
#include "core/application/Engine.h"
#include "core/graphics/ImageCube.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/CommandPool.h"


#define MAX_SPECULAR_MIP_LEVELS 8

struct DiffuseIrradianceComputeUBO {
    glm::uvec2 srcMapSize;
    glm::uvec2 dstMapSize;
};

struct PrefilteredEnvironmentComputePushConstants {
    uint32_t srcSize;
    uint32_t dstSize;
    uint32_t mipLevel;
    uint32_t numMipLevels;
};

std::shared_ptr<Image2D> EnvironmentMap::s_BRDFIntegrationMapImage = nullptr;
std::shared_ptr<Texture> EnvironmentMap::s_BRDFIntegrationMap = nullptr;
ComputePipeline* EnvironmentMap::s_diffuseIrradianceConvolutionComputePipeline = nullptr;
DescriptorSet* EnvironmentMap::s_diffuseIrradianceConvolutionDescriptorSet = nullptr;
ComputePipeline* EnvironmentMap::s_prefilteredEnvironmentComputePipeline = nullptr;
DescriptorSet* EnvironmentMap::s_prefilteredEnvironmentDescriptorSet = nullptr;
ComputePipeline* EnvironmentMap::s_BRDFIntegrationMapComputePipeline = nullptr;
DescriptorSet* EnvironmentMap::s_BRDFIntegrationMapDescriptorSet = nullptr;
Buffer* EnvironmentMap::s_uniformBuffer = nullptr;



EnvironmentMap::EnvironmentMap(const uint32_t& irradianceMapSize, const uint32_t& specularMapSize):
    m_irradianceMapSize(irradianceMapSize),
    m_specularMapSize(specularMapSize),
    m_specularMapMipLevels(5),
    m_needsRecompute(false) {
}

EnvironmentMap::~EnvironmentMap() {
    m_specularReflectionMapTextureMipLevels.clear();
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

        bool recreateDiffuseImage = m_diffuseIrradianceImage == nullptr || m_diffuseIrradianceImage->getSize() != m_irradianceMapSize;
        bool recreateSpecularImage = m_specularReflectionImage == nullptr || m_specularReflectionImage->getSize() != m_specularMapSize;

        if (recreateDiffuseImage || recreateSpecularImage) {
            ImageCubeConfiguration imageConfig;
            imageConfig.device = Engine::graphics()->getDevice();
            imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
            imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
            imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

            if (recreateDiffuseImage) {
                imageConfig.size = m_irradianceMapSize;
                imageConfig.mipLevels = 1;
                imageConfig.generateMipmap = false;
                m_diffuseIrradianceImage = std::shared_ptr<ImageCube>(ImageCube::create(imageConfig));
                if (m_diffuseIrradianceImage == nullptr) {
                    printf("Failed to create diffuse irradiance ImageCube for environment map\n");
                    assert(false);
                    return;
                }
                m_diffuseIrradianceMapTexture = createTexture(m_diffuseIrradianceImage, 0, UINT32_MAX);
            }

            if (recreateSpecularImage) {
                imageConfig.size = m_specularMapSize;
                imageConfig.mipLevels = m_specularMapMipLevels;
                imageConfig.generateMipmap = false;
                m_specularReflectionImage = std::shared_ptr<ImageCube>(ImageCube::create(imageConfig));
                if (m_specularReflectionImage == nullptr) {
                    printf("Failed to create specular reflection ImageCube for environment map\n");
                    assert(false);
                    return;
                }

                m_specularReflectionMapTexture = createTexture(m_specularReflectionImage, 0, UINT32_MAX);

                m_specularReflectionMapTextureMipLevels.clear();
                m_specularReflectionMapTextureMipLevels.resize(m_specularMapMipLevels, nullptr);

                for (size_t i = 0; i < m_specularMapMipLevels; ++i) {
                    m_specularReflectionMapTextureMipLevels[i] = createTexture(m_specularReflectionImage, i, 1);
                }
            }
        }

        const std::shared_ptr<CommandPool>& commandPool = Engine::graphics()->commandPool();
        const vk::CommandBuffer& commandBuffer = **commandPool->getOrCreateCommandBuffer("compute_main", { vk::CommandBufferLevel::ePrimary });
        const vk::Queue& computeQueue = **Engine::graphics()->getQueue(QUEUE_COMPUTE_MAIN);

        vk::CommandBufferBeginInfo commandBeginInfo;
        commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer.begin(commandBeginInfo);

        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 6);

        ImageTransitionState updateState = ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader);
        ImageTransitionState finalState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);

        // Transition all mip-levels of diffuse and specular maps for shader write operations
        subresourceRange.setLevelCount(m_diffuseIrradianceImage->getMipLevelCount());
        ImageUtil::transitionLayout(commandBuffer, m_diffuseIrradianceImage->getImage(), subresourceRange, ImageTransition::FromAny(), updateState);
        subresourceRange.setLevelCount(m_specularReflectionImage->getMipLevelCount());
        ImageUtil::transitionLayout(commandBuffer, m_specularReflectionImage->getImage(), subresourceRange, ImageTransition::FromAny(), updateState);

        calculateDiffuseIrradiance(commandBuffer);
        calculateSpecularReflection(commandBuffer);

        if (s_BRDFIntegrationMap == nullptr)
            calculateBRDFIntegrationMap(commandBuffer);

        // Transition all mip-levels of diffuse and specular maps for optimal shader read operations
        subresourceRange.setLevelCount(m_diffuseIrradianceImage->getMipLevelCount());
        ImageUtil::transitionLayout(commandBuffer, m_diffuseIrradianceImage->getImage(), subresourceRange, updateState, finalState);
        subresourceRange.setLevelCount(m_specularReflectionImage->getMipLevelCount());
        ImageUtil::transitionLayout(commandBuffer, m_specularReflectionImage->getImage(), subresourceRange, updateState, finalState);

        commandBuffer.end();

        vk::SubmitInfo queueSubmitInfo;
        queueSubmitInfo.setCommandBufferCount(1);
        queueSubmitInfo.setPCommandBuffers(&commandBuffer);
        computeQueue.submit(1, &queueSubmitInfo, VK_NULL_HANDLE);
        computeQueue.waitIdle();

        printf("======== Updating environment map took %.2f msec\n", Performance::milliseconds(t0));
    }
}

void EnvironmentMap::setEnvironmentImage(const std::shared_ptr<ImageCube>& environmentImage) {
    m_environmentImage = environmentImage;
    m_environmentMapTexture = createTexture(environmentImage, 0, UINT32_MAX);
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

std::shared_ptr<Texture> EnvironmentMap::getBRDFIntegrationMap() {
    return s_BRDFIntegrationMap;
}

void EnvironmentMap::calculateDiffuseIrradiance(const vk::CommandBuffer& commandBuffer) const {
    PROFILE_SCOPE("EnvironmentMap::calculateDiffuseIrradiance");

    printf("Recomputing diffuse-irradiance environment map\n");

    ComputePipeline* computePipeline = getDiffuseIrradianceComputePipeline();
    DescriptorSet* descriptorSet = getDiffuseIrradianceComputeDescriptorSet();
    Buffer* uniformBuffer = getUniformBuffer();

    constexpr vk::DeviceSize uboOffset = 0;

    DiffuseIrradianceComputeUBO uniformData{};
    uniformData.srcMapSize.x = m_environmentImage->getWidth();
    uniformData.srcMapSize.y = m_environmentImage->getHeight();
    uniformData.dstMapSize.x = m_irradianceMapSize;
    uniformData.dstMapSize.y = m_irradianceMapSize;
    uniformBuffer->upload(uboOffset, sizeof(DiffuseIrradianceComputeUBO), &uniformData);

    DescriptorSetWriter(descriptorSet)
    .writeImage(1, m_environmentMapTexture.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
    .writeImage(2, m_diffuseIrradianceMapTexture.get(), vk::ImageLayout::eGeneral) // is eGeneral the only option here? Nothing for ShaderWriteOnly use case? :(
    .write();

    constexpr uint32_t workgroupSize = 1;

    computePipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,computePipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSet(), 0, nullptr);

    const uint32_t workgroupCountXY = (uint32_t) glm::ceil(m_irradianceMapSize / workgroupSize);
    computePipeline->dispatch(commandBuffer, workgroupCountXY, workgroupCountXY, 6);

}

void EnvironmentMap::calculateSpecularReflection(const vk::CommandBuffer& commandBuffer) const {
    PROFILE_SCOPE("EnvironmentMap::calculateSpecularReflection");

    printf("Recomputing specular reflection prefiltered environment map\n");

    ComputePipeline* computePipeline = getPrefilteredEnvironmentComputePipeline();
    DescriptorSet* descriptorSet = getPrefilteredEnvironmentComputeDescriptorSet();

    PrefilteredEnvironmentComputePushConstants pushConstantData{};
    pushConstantData.srcSize = m_environmentImage->getSize();
    pushConstantData.numMipLevels = m_specularMapMipLevels;

    std::vector<Texture*> mipLevelImages(MAX_SPECULAR_MIP_LEVELS, nullptr);
    std::vector<vk::ImageLayout> mipLevelImageLayouts(MAX_SPECULAR_MIP_LEVELS, vk::ImageLayout::eGeneral);
    for (uint32_t i = 0; i < MAX_SPECULAR_MIP_LEVELS; ++i)
        mipLevelImages[i] = m_specularReflectionMapTextureMipLevels[glm::min(i, m_specularMapMipLevels - 1)].get();

    DescriptorSetWriter(descriptorSet)
        .writeImage(0, m_environmentMapTexture.get(), vk::ImageLayout::eShaderReadOnlyOptimal)
        .writeImage(1, mipLevelImages.data(), mipLevelImageLayouts.data(), MAX_SPECULAR_MIP_LEVELS, 0)
        .write();

    constexpr uint32_t workgroupSize = 16;

    computePipeline->bind(commandBuffer);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,computePipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSet(), 0, nullptr);

    uint32_t specularMapSize = m_specularMapSize;
    for (size_t i = 0; i < m_specularMapMipLevels; ++i) {
        pushConstantData.dstSize = specularMapSize;
        pushConstantData.mipLevel = i;

        commandBuffer.pushConstants(computePipeline->getPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilteredEnvironmentComputePushConstants), &pushConstantData);

        const uint32_t workgroupCountXY = (uint32_t) glm::ceil(specularMapSize / workgroupSize);
        computePipeline->dispatch(commandBuffer, workgroupCountXY, workgroupCountXY, 6);
        specularMapSize >>= 1;
    }
}

void EnvironmentMap::calculateBRDFIntegrationMap(const vk::CommandBuffer& commandBuffer) {
    if (s_BRDFIntegrationMap == nullptr) {
        constexpr uint32_t BRDFIntegrationMapSize = 512;
        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);

        struct BRDFIntegrationPushConstants {
            glm::uvec2 dstSize;
        };

        Image2DConfiguration imageConfig{};
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.setSize(BRDFIntegrationMapSize, BRDFIntegrationMapSize);
        imageConfig.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;
        imageConfig.format = vk::Format::eR16G16B16A16Sfloat;
        imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        imageConfig.generateMipmap = false;
        imageConfig.mipLevels = 1;

        s_BRDFIntegrationMapImage = std::shared_ptr<Image2D>(Image2D::create(imageConfig));
        assert(s_BRDFIntegrationMapImage != nullptr);

        ImageViewConfiguration imageViewConfig{};
        imageViewConfig.device = Engine::graphics()->getDevice();
        imageViewConfig.format = s_BRDFIntegrationMapImage->getFormat();
        imageViewConfig.setImage(s_BRDFIntegrationMapImage.get());
        imageViewConfig.baseMipLevel = 0;
        imageViewConfig.mipLevelCount = 1;
        imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;

        SamplerConfiguration samplerConfig{};
        samplerConfig.device = Engine::graphics()->getDevice();
        samplerConfig.minFilter = vk::Filter::eLinear;
        samplerConfig.magFilter = vk::Filter::eLinear;
        samplerConfig.minLod = 0.0F;
        samplerConfig.maxLod = 0.0F;

        s_BRDFIntegrationMap = std::shared_ptr<Texture>(Texture::create(imageViewConfig, samplerConfig));
        assert(s_BRDFIntegrationMap != nullptr);

        if (s_BRDFIntegrationMapDescriptorSet == nullptr) {
            std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                    .addStorageImage(0, vk::ShaderStageFlagBits::eCompute).build();
            s_BRDFIntegrationMapDescriptorSet = DescriptorSet::create(descriptorSetLayout,Engine::graphics()->descriptorPool());
            assert(s_BRDFIntegrationMapDescriptorSet != nullptr);
        }

        if (s_BRDFIntegrationMapComputePipeline == nullptr) {
            ComputePipelineConfiguration pipelineConfig;
            pipelineConfig.device = Engine::graphics()->getDevice();
            pipelineConfig.computeShader = "res/shaders/compute/compute_BRDFIntegrationMap.glsl";
            pipelineConfig.addDescriptorSetLayout(s_BRDFIntegrationMapDescriptorSet->getLayout().get());
            pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(BRDFIntegrationPushConstants));
            s_BRDFIntegrationMapComputePipeline = ComputePipeline::create(pipelineConfig);
            assert(s_BRDFIntegrationMapComputePipeline != nullptr);
        }

        ImageTransitionState updateState = ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader);
        ImageTransitionState finalState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);

        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        // Transition image for shader write operations
        ImageUtil::transitionLayout(commandBuffer, s_BRDFIntegrationMapImage->getImage(), subresourceRange, ImageTransition::FromAny(), updateState);

        ComputePipeline* computePipeline = s_BRDFIntegrationMapComputePipeline;
        DescriptorSet* descriptorSet = s_BRDFIntegrationMapDescriptorSet;

        DescriptorSetWriter(descriptorSet)
            .writeImage(0, s_BRDFIntegrationMap.get(), vk::ImageLayout::eGeneral)
            .write();

        constexpr uint32_t workgroupSize = 16;

        computePipeline->bind(commandBuffer);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,computePipeline->getPipelineLayout(), 0, 1, &descriptorSet->getDescriptorSet(), 0, nullptr);

        BRDFIntegrationPushConstants pushConstantData{};
        pushConstantData.dstSize = glm::uvec2(BRDFIntegrationMapSize);
        commandBuffer.pushConstants(computePipeline->getPipelineLayout(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(BRDFIntegrationPushConstants), &pushConstantData);

        const uint32_t workgroupCountXY = (uint32_t) glm::ceil(BRDFIntegrationMapSize / workgroupSize);
        computePipeline->dispatch(commandBuffer, workgroupCountXY, workgroupCountXY, 1);

        // Transition image for shader read operations - This also acts as the memory barrier for subsequent accesses to this image
        ImageUtil::transitionLayout(commandBuffer, s_BRDFIntegrationMapImage->getImage(), subresourceRange, updateState, finalState);
    }
}

DescriptorSet* EnvironmentMap::getDiffuseIrradianceComputeDescriptorSet() {
    if (s_diffuseIrradianceConvolutionDescriptorSet == nullptr) {
        std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                .addUniformBuffer(0, vk::ShaderStageFlagBits::eCompute)
                .addCombinedImageSampler(1, vk::ShaderStageFlagBits::eCompute)
                .addStorageImage(2, vk::ShaderStageFlagBits::eCompute)
                .build();
        s_diffuseIrradianceConvolutionDescriptorSet = DescriptorSet::create(descriptorSetLayout,Engine::graphics()->descriptorPool());
        assert(s_diffuseIrradianceConvolutionDescriptorSet != nullptr);

        constexpr vk::DeviceSize uboOffset = 0;
        DescriptorSetWriter(s_diffuseIrradianceConvolutionDescriptorSet)
                .writeBuffer(0, getUniformBuffer(), uboOffset, sizeof(DiffuseIrradianceComputeUBO))
                .write();

        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_diffuseIrradianceConvolutionDescriptorSet;
}

ComputePipeline* EnvironmentMap::getDiffuseIrradianceComputePipeline() {
    if (s_diffuseIrradianceConvolutionComputePipeline == nullptr) {
        ComputePipelineConfiguration pipelineConfig;
        pipelineConfig.device = Engine::graphics()->getDevice();
        pipelineConfig.computeShader = "res/shaders/compute/compute_diffuseIrradiance.glsl";
        pipelineConfig.addDescriptorSetLayout(getDiffuseIrradianceComputeDescriptorSet()->getLayout().get());
        s_diffuseIrradianceConvolutionComputePipeline = ComputePipeline::create(pipelineConfig);
        assert(s_diffuseIrradianceConvolutionComputePipeline != nullptr);

        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_diffuseIrradianceConvolutionComputePipeline;
}

DescriptorSet* EnvironmentMap::getPrefilteredEnvironmentComputeDescriptorSet() {
    if (s_prefilteredEnvironmentDescriptorSet == nullptr) {
        std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eCompute)
                .addStorageImage(1, vk::ShaderStageFlagBits::eCompute, MAX_SPECULAR_MIP_LEVELS)
                .build();
        s_prefilteredEnvironmentDescriptorSet = DescriptorSet::create(descriptorSetLayout,Engine::graphics()->descriptorPool());
        assert(s_prefilteredEnvironmentDescriptorSet != nullptr);

        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_prefilteredEnvironmentDescriptorSet;
}

ComputePipeline* EnvironmentMap::getPrefilteredEnvironmentComputePipeline() {
    if (s_prefilteredEnvironmentComputePipeline == nullptr) {
        ComputePipelineConfiguration pipelineConfig;
        pipelineConfig.device = Engine::graphics()->getDevice();
        pipelineConfig.computeShader = "res/shaders/compute/compute_prefilterEnvMap.glsl";
        pipelineConfig.addDescriptorSetLayout(getPrefilteredEnvironmentComputeDescriptorSet()->getLayout().get());
        pipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(PrefilteredEnvironmentComputePushConstants));
        s_prefilteredEnvironmentComputePipeline = ComputePipeline::create(pipelineConfig);
        assert(s_prefilteredEnvironmentComputePipeline != nullptr);

        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_prefilteredEnvironmentComputePipeline;
}

Buffer* EnvironmentMap::getUniformBuffer() {
    if (s_uniformBuffer == nullptr) {
        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
        bufferConfig.size = ROUND_TO_MULTIPLE(sizeof(DiffuseIrradianceComputeUBO), 256);
        delete s_uniformBuffer;
        s_uniformBuffer = Buffer::create(bufferConfig);
        assert(s_uniformBuffer != nullptr);

        Engine::eventDispatcher()->connect(&EnvironmentMap::onCleanupGraphics);
    }

    return s_uniformBuffer;
}


std::shared_ptr<Texture> EnvironmentMap::createTexture(const std::shared_ptr<ImageCube>& image, const uint32_t& baseMipLevel, const uint32_t mipLevelCount) const {
    if (image == nullptr)
        return nullptr;

    ImageViewConfiguration imageViewConfig;
    imageViewConfig.device = Engine::graphics()->getDevice();
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(image.get());
    imageViewConfig.format = image->getFormat();
    imageViewConfig.baseArrayLayer = 0;
    imageViewConfig.arrayLayerCount = 6;
    imageViewConfig.baseMipLevel = baseMipLevel;
    imageViewConfig.mipLevelCount = glm::min(mipLevelCount, image->getMipLevelCount() - baseMipLevel);

    SamplerConfiguration samplerConfig;
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerConfig.minLod = 0.0F;
    samplerConfig.maxLod = image->getMipLevelCount();
    samplerConfig.mipLodBias = 0.0F;
    return std::shared_ptr<Texture>(Texture::create(imageViewConfig, samplerConfig));
}

void EnvironmentMap::onCleanupGraphics(const ShutdownGraphicsEvent& event) {
    delete s_uniformBuffer;
    delete s_BRDFIntegrationMapDescriptorSet;
    delete s_BRDFIntegrationMapComputePipeline;
    delete s_diffuseIrradianceConvolutionDescriptorSet;
    delete s_diffuseIrradianceConvolutionComputePipeline;
    delete s_prefilteredEnvironmentDescriptorSet;
    delete s_prefilteredEnvironmentComputePipeline;
    s_BRDFIntegrationMapImage.reset();
    s_BRDFIntegrationMap.reset();
}