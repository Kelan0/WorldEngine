
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/renderer/RenderCamera.h"


#define GAUSSIAN_BLUE_DIRECTION_X 0
#define GAUSSIAN_BLUE_DIRECTION_Y 1


struct GaussianBlurPushConstants {
    glm::uvec2 srcSize;
    glm::uvec2 dstSize;
    float blurRadius;
    uint32_t blurDirection;
};


LightRenderer::LightRenderer():
    m_vsmBlurIntermediateImageView(nullptr),
    m_vsmBlurIntermediateImage(nullptr),
    m_maxVisibleShadowMaps(32) {
}

LightRenderer::~LightRenderer() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_shadowRenderPassResources[i]->descriptorSet;
        delete m_shadowRenderPassResources[i]->cameraInfoBuffer;
        delete m_lightingRenderPassResources[i]->descriptorSet;
        delete m_lightingRenderPassResources[i]->lightInfoBuffer;
        delete m_lightingRenderPassResources[i]->shadowMapBuffer;
    }
    delete m_testDirectionShadowMap;
    m_shadowGraphicsPipeline.reset();
    m_shadowRenderPass.reset();
    m_shadowRenderPassDescriptorSetLayout.reset();
    m_lightingRenderPassDescriptorSetLayout.reset();
    m_emptyShadowMapImage.reset();
    m_emptyShadowMap.reset();

    delete m_vsmBlurXComputeDescriptorSet;
    delete m_vsmBlurYComputeDescriptorSet;
    delete m_vsmBlurIntermediateImageView;
    delete m_vsmBlurIntermediateImage;
}

bool LightRenderer::init() {

    initEmptyShadowMap();

    m_shadowRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex, true)
            .build();

    m_lightingRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addStorageBuffer(0, vk::ShaderStageFlagBits::eFragment)
            .addStorageBuffer(1, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(2, vk::ShaderStageFlagBits::eFragment, MAX_SHADOW_MAPS)
            .build();

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_shadowRenderPassResources.set(i, new ShadowRenderPassResources());
        m_lightingRenderPassResources.set(i, new LightingRenderPassResources());
        m_shadowRenderPassResources[i]->cameraInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->lightInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->shadowMapBuffer = nullptr;

        m_shadowRenderPassResources[i]->descriptorSet = DescriptorSet::create(m_shadowRenderPassDescriptorSetLayout, Engine::graphics()->descriptorPool());
        if (m_shadowRenderPassResources[i]->descriptorSet == nullptr) {
            printf("LightRenderer::init - Failed to create camera info descriptor set\n");
            return false;
        }

        m_lightingRenderPassResources[i]->descriptorSet = DescriptorSet::create(m_lightingRenderPassDescriptorSetLayout, Engine::graphics()->descriptorPool());
        if (m_lightingRenderPassResources[i]->descriptorSet == nullptr) {
            printf("LightRenderer::init - Failed to create camera info descriptor set\n");
            return false;
        }

        std::vector<Texture*> emptyShadowMapTextures(MAX_SHADOW_MAPS, m_emptyShadowMap.get());
        std::vector<vk::ImageLayout> emptyShadowMapImageLayouts(MAX_SHADOW_MAPS, vk::ImageLayout::eShaderReadOnlyOptimal);
        DescriptorSetWriter(m_lightingRenderPassResources[i]->descriptorSet)
            .writeImage(2, emptyShadowMapTextures.data(), emptyShadowMapImageLayouts.data(), MAX_SHADOW_MAPS)
            .write();
    }

    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, 2> attachments;

    attachments[0].setFormat(vk::Format::eR32G32B32A32Sfloat);
    attachments[0].setSamples(samples);
    attachments[0].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[0].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[0].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[0].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[0].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[0].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[1].setFormat(vk::Format::eD32Sfloat);
    attachments[1].setSamples(samples);
    attachments[1].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[1].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[1].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[1].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[1].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[1].setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    SubpassConfiguration subpassConfiguration;
    subpassConfiguration.addColourAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.setDepthStencilAttachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    RenderPassConfiguration renderPassConfig;
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.setClearColour(0, glm::vec4(1.0F));
    renderPassConfig.setClearDepth(1, 1.0F);

    m_shadowRenderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig));
    if (!m_shadowRenderPass) {
        printf("LightRenderer::init - Failed to create render pass\n");
        return false;
    }

    GraphicsPipelineConfiguration pipelineConfig;
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_shadowRenderPass;
    pipelineConfig.setViewport(512, 512);
    pipelineConfig.vertexShader = "res/shaders/shadow/shadow.vert";
    pipelineConfig.fragmentShader = "res/shaders/shadow/shadow.frag";
    pipelineConfig.vertexInputBindings = MeshUtils::getVertexBindingDescriptions<Vertex>();
    pipelineConfig.vertexInputAttributes = MeshUtils::getVertexAttributeDescriptions<Vertex>();
    pipelineConfig.addDescriptorSetLayout(m_shadowRenderPassDescriptorSetLayout->getDescriptorSetLayout());
    pipelineConfig.addDescriptorSetLayout(Engine::sceneRenderer()->getObjectDescriptorSetLayout()->getDescriptorSetLayout());
    pipelineConfig.setDynamicState(vk::DynamicState::eViewport, true);
    pipelineConfig.setDynamicState(vk::DynamicState::eScissor, true);
    pipelineConfig.setAttachmentBlendState(0, AttachmentBlendState(false, 0b1111));
    pipelineConfig.frontFace = vk::FrontFace::eCounterClockwise;
//    pipelineConfig.depthBiasEnable = true;
//    pipelineConfig.depthBias.constant = 200.0F;
//    pipelineConfig.depthBias.slope = 1.5F;

    m_shadowGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(pipelineConfig));
    if (!m_shadowGraphicsPipeline) {
        printf("LightRenderer::init - Failed to create graphics pipeline\n");
        return false;
    }

    m_vsmBlurComputeDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eCompute)
            .addStorageImage(1, vk::ShaderStageFlagBits::eCompute)
            .build();

    m_vsmBlurXComputeDescriptorSet = DescriptorSet::create(m_vsmBlurComputeDescriptorSetLayout, Engine::graphics()->descriptorPool());
    m_vsmBlurYComputeDescriptorSet = DescriptorSet::create(m_vsmBlurComputeDescriptorSetLayout, Engine::graphics()->descriptorPool());

    ComputePipelineConfiguration computePipelineConfig;
    computePipelineConfig.device = Engine::graphics()->getDevice();
    computePipelineConfig.computeShader = "res/shaders/compute/compute_gaussianBlur.glsl";
    computePipelineConfig.addDescriptorSetLayout(m_vsmBlurComputeDescriptorSetLayout.get());
    computePipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants));
    m_vsmBlurComputePipeline = std::shared_ptr<ComputePipeline>(ComputePipeline::create(computePipelineConfig));



    SamplerConfiguration samplerConfig;
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.wrapV = vk::SamplerAddressMode::eClampToEdge;
    m_vsmShadowMapSampler = std::shared_ptr<Sampler>(Sampler::create(samplerConfig));


    m_testDirectionShadowMap = new DirectionShadowMap(1024, 1024);
    m_testDirectionShadowMap->setDirection(glm::vec3(-1.0F, -1.3F, -1.0F));

    return true;
}

void LightRenderer::render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("LightRenderer::render");

    m_testDirectionShadowMap->update();
    m_testDirectionShadowMap->begin(commandBuffer, m_shadowRenderPass.get());

    uint32_t w = m_testDirectionShadowMap->getResolution().x;
    uint32_t h = m_testDirectionShadowMap->getResolution().y;

    m_shadowGraphicsPipeline->setViewport(commandBuffer, 0, 0.0F, 0.0F, (float)w, (float)h, 0.0F, 1.0F);
    m_shadowGraphicsPipeline->setScissor(commandBuffer, 0, 0, 0, w, h);


    updateCameraInfoBuffer(1);
    updateLightInfoBuffer(32);
    updateShadowMapInfoBuffer(MAX_SHADOW_MAPS);

    RenderCamera shadowCamera;
    calculateShadowRenderCamera(renderCamera, m_testDirectionShadowMap, 0.0F, 4.0F, &shadowCamera);

    GPULight lightInfoData{};
    lightInfoData.type = LightType_Directional;
    lightInfoData.worldDirection = glm::vec4(m_testDirectionShadowMap->getDirection(), 0.0F);
    lightInfoData.intensity = glm::vec4(32.0F, 32.0F, 32.0F, 0.0F);
    lightInfoData.shadowMapIndex = 0;


    GPUShadowMap cameraInfoData{};
    cameraInfoData.viewProjectionMatrix = shadowCamera.getViewProjectionMatrix();
    m_shadowRenderPassResources->cameraInfoBuffer->upload(0, sizeof(GPUShadowMap), &cameraInfoData);
    m_lightingRenderPassResources->shadowMapBuffer->upload(0, sizeof(GPUShadowMap), &cameraInfoData);
    m_lightingRenderPassResources->lightInfoBuffer->upload(0, sizeof(GPULight), &lightInfoData);

    vk::DescriptorSet descriptorSets[2] = {
            m_shadowRenderPassResources->descriptorSet->getDescriptorSet(),
            Engine::sceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
    };

    uint32_t dynamicOffsets[1] = { 0 };

    m_shadowGraphicsPipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shadowGraphicsPipeline->getPipelineLayout(), 0, 2, descriptorSets, 1, dynamicOffsets);

    Engine::sceneRenderer()->render(dt, commandBuffer, &shadowCamera);

    commandBuffer.endRenderPass();

    blurImage(commandBuffer, m_vsmShadowMapSampler.get(), m_testDirectionShadowMap->getShadowVarianceImageView(), m_testDirectionShadowMap->getResolution(), m_testDirectionShadowMap->getShadowVarianceImageView(), m_testDirectionShadowMap->getResolution(), glm::vec2(10.0));

    DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
            .writeImage(2, m_vsmShadowMapSampler.get(), m_testDirectionShadowMap->getShadowVarianceImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0)
            .write();
}

const std::shared_ptr<RenderPass>& LightRenderer::getRenderPass() const {
    return m_shadowRenderPass;
}

DirectionShadowMap* LightRenderer::getTestShadowMap() const {
    return m_testDirectionShadowMap;
}

const uint32_t& LightRenderer::getMaxVisibleShadowMaps() const {
    return m_maxVisibleShadowMaps;
}

const std::shared_ptr<Texture>& LightRenderer::getEmptyShadowMap() const {
    return m_emptyShadowMap;
}

const std::shared_ptr<DescriptorSetLayout>& LightRenderer::getLightingRenderPassDescriptorSetLayout() const {
    return m_lightingRenderPassDescriptorSetLayout;
}

DescriptorSet* LightRenderer::getLightingRenderPassDescriptorSet() const {
    return m_lightingRenderPassResources->descriptorSet;
}

void LightRenderer::initEmptyShadowMap() {
    union {
        float depth;
        uint8_t pixelData[4];
    };
    depth = 1.0F;
    ImageData imageData(pixelData, 1, 1, ImagePixelLayout::R, ImagePixelFormat::Float32);

    Image2DConfiguration imageConfig;
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.format = vk::Format::eR32Sfloat;
    imageConfig.setSize(1, 1);
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled;
    imageConfig.imageData = &imageData;
    m_emptyShadowMapImage = std::shared_ptr<Image2D>(Image2D::create(imageConfig));

    ImageViewConfiguration emptyShadowMapImageViewConfig;
    emptyShadowMapImageViewConfig.device = Engine::graphics()->getDevice();
    emptyShadowMapImageViewConfig.format = vk::Format::eR32Sfloat;
    emptyShadowMapImageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    emptyShadowMapImageViewConfig.setImage(m_emptyShadowMapImage.get());

    SamplerConfiguration samplerConfig;
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eNearest;
    samplerConfig.magFilter = vk::Filter::eNearest;

    m_emptyShadowMap = std::shared_ptr<Texture>(Texture::create(emptyShadowMapImageViewConfig, samplerConfig));
}

void LightRenderer::updateCameraInfoBuffer(const size_t& maxShadowLights) {
    PROFILE_SCOPE("LightRenderer::updateCameraInfoBuffer");

    vk::DeviceSize newBufferSize = sizeof(CameraInfoUBO) * maxShadowLights;

    if (m_shadowRenderPassResources->cameraInfoBuffer == nullptr || newBufferSize > m_shadowRenderPassResources->cameraInfoBuffer->getSize()) {
        PROFILE_SCOPE("Allocate CameraInfoBuffer");

        printf("Allocating CameraInfoBuffer - %llu objects\n", maxShadowLights);

        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = newBufferSize;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_shadowRenderPassResources->cameraInfoBuffer;
        m_shadowRenderPassResources->cameraInfoBuffer = Buffer::create(bufferConfig);

        DescriptorSetWriter(m_shadowRenderPassResources->descriptorSet)
                .writeBuffer(0, m_shadowRenderPassResources->cameraInfoBuffer, 0, newBufferSize)
                .write();
    }
}

void LightRenderer::updateLightInfoBuffer(const size_t& maxLights) {
    PROFILE_SCOPE("LightRenderer::updateLightInfoBuffer");

    vk::DeviceSize newBufferSize = sizeof(GPULight) * maxLights;

    if (m_lightingRenderPassResources->lightInfoBuffer == nullptr || newBufferSize > m_lightingRenderPassResources->lightInfoBuffer->getSize()) {
        PROFILE_SCOPE("Allocate updateLightInfoBuffer");

        printf("Allocating LightInfoBuffer - %llu objects\n", maxLights);

        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = newBufferSize;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_lightingRenderPassResources->lightInfoBuffer;
        m_lightingRenderPassResources->lightInfoBuffer = Buffer::create(bufferConfig);

        DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                .writeBuffer(0, m_lightingRenderPassResources->lightInfoBuffer, 0, newBufferSize)
                .write();
    }
}

void LightRenderer::updateShadowMapInfoBuffer(const size_t& maxShadowLights) {
    PROFILE_SCOPE("LightRenderer::updateShadowMapInfoBuffer");

    vk::DeviceSize newBufferSize = sizeof(GPUShadowMap) * maxShadowLights;

    if (m_lightingRenderPassResources->shadowMapBuffer == nullptr || newBufferSize > m_lightingRenderPassResources->shadowMapBuffer->getSize()) {
        PROFILE_SCOPE("Allocate ShadowMapInfoBuffer");

        printf("Allocating ShadowMapInfoBuffer - %llu objects\n", maxShadowLights);

        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = newBufferSize;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_lightingRenderPassResources->shadowMapBuffer;
        m_lightingRenderPassResources->shadowMapBuffer = Buffer::create(bufferConfig);

        DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                .writeBuffer(1, m_lightingRenderPassResources->shadowMapBuffer, 0, newBufferSize)
                .write();
    }
}

void LightRenderer::blurImage(const vk::CommandBuffer& commandBuffer, const Sampler* sampler, const ImageView* srcImage, const glm::uvec2& srcSize, const ImageView* dstImage, const glm::uvec2& dstSize, const glm::vec2& blurRadius) {
    PROFILE_SCOPE("LightRenderer::blurImage")

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    const vk::PipelineLayout& pipelineLayout = m_vsmBlurComputePipeline->getPipelineLayout();

    GaussianBlurPushConstants pushConstantData{};
    pushConstantData.srcSize = srcSize;
    pushConstantData.dstSize = dstSize;

    m_vsmBlurComputePipeline->bind(commandBuffer);

    if (m_vsmBlurIntermediateImage == nullptr || m_vsmBlurIntermediateImage->getWidth() != dstSize.x || m_vsmBlurIntermediateImage->getHeight() != dstSize.y) {
        delete m_vsmBlurIntermediateImageView;
        delete m_vsmBlurIntermediateImage;

        Image2DConfiguration imageConfig;
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
        imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
        imageConfig.setSize(dstSize);
        m_vsmBlurIntermediateImage = Image2D::create(imageConfig);

        ImageViewConfiguration imageViewConfig;
        imageViewConfig.device = Engine::graphics()->getDevice();
        imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
        imageViewConfig.setImage(m_vsmBlurIntermediateImage);
        m_vsmBlurIntermediateImageView = ImageView::create(imageViewConfig);

        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
                                    ImageTransition::FromAny(),
                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
    } else {

        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader),
                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
    }
    ImageUtil::transitionLayout(commandBuffer, srcImage->getImage(), subresourceRange,
                                ImageTransition::FromAny(),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));

    DescriptorSetWriter(m_vsmBlurXComputeDescriptorSet)
            .writeImage(0, sampler, srcImage, vk::ImageLayout::eShaderReadOnlyOptimal)
            .writeImage(1, sampler, m_vsmBlurIntermediateImageView, vk::ImageLayout::eGeneral)
            .write();

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurXComputeDescriptorSet->getDescriptorSet(), 0, nullptr);

    pushConstantData.blurRadius = blurRadius.x;
    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_X;
    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
    m_vsmBlurComputePipeline->dispatch(commandBuffer, INT_DIV_CEIL(dstSize.x, 16), INT_DIV_CEIL(dstSize.y, 16), 1);






    ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader));
    ImageUtil::transitionLayout(commandBuffer, dstImage->getImage(), subresourceRange,
                                ImageTransition::FromAny(),
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));

    DescriptorSetWriter(m_vsmBlurYComputeDescriptorSet)
            .writeImage(0, sampler, m_vsmBlurIntermediateImageView, vk::ImageLayout::eShaderReadOnlyOptimal)
            .writeImage(1, sampler, dstImage, vk::ImageLayout::eGeneral)
            .write();

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurYComputeDescriptorSet->getDescriptorSet(), 0, nullptr);

    pushConstantData.blurRadius = blurRadius.y;
    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_Y;
    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
    m_vsmBlurComputePipeline->dispatch(commandBuffer, INT_DIV_CEIL(dstSize.x, 16), INT_DIV_CEIL(dstSize.y, 16), 1);

    ImageUtil::transitionLayout(commandBuffer, dstImage->getImage(), subresourceRange,
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader));
}

void LightRenderer::calculateShadowRenderCamera(const RenderCamera* viewerRenderCamera, const DirectionShadowMap* shadowMap, const double& nearDistance, const double& farDistance, RenderCamera* outShadowRenderCamera) {
    PROFILE_SCOPE("LightRenderer::calculateShadowRenderCamera");

    std::array<glm::dvec3, Frustum::NumCorners> viewerFrustumCorners = Frustum::getCornersNDC();
    glm::dvec4 temp;
    // Calculate the world-space corners of the viewers frustum
    for (size_t i = 0; i < Frustum::NumCorners; ++i) {
        temp = viewerRenderCamera->getInverseViewProjectionMatrix() * glm::dvec4(viewerFrustumCorners[i], 1.0);
        viewerFrustumCorners[i] = glm::dvec3(temp) / temp.w;
    }

    // Adjust the corners to match the near/far distance provided. We divide by the dot product so that the distance is
    // independent of the cameras FOV (i.e. the distance is along the center vector pointing along the cameras Z axis)
    glm::dvec3 dTL = glm::normalize(viewerFrustumCorners[Frustum::Corner_Left_Top_Far] - viewerFrustumCorners[Frustum::Corner_Left_Top_Near]);
    glm::dvec3 dTR = glm::normalize(viewerFrustumCorners[Frustum::Corner_Right_Top_Far] - viewerFrustumCorners[Frustum::Corner_Right_Top_Near]);
    glm::dvec3 dBR = glm::normalize(viewerFrustumCorners[Frustum::Corner_Right_Bottom_Far] - viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near]);
    glm::dvec3 dBL = glm::normalize(viewerFrustumCorners[Frustum::Corner_Left_Bottom_Far] - viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near]);
    glm::dvec3 dC = glm::normalize(dTL + dTR + dBR + dBL);
    double scaleTL = 1.0 / glm::dot(dTL, dC);
    double scaleTR = 1.0 / glm::dot(dTR, dC);
    double scaleBR = 1.0 / glm::dot(dBR, dC);
    double scaleBL = 1.0 / glm::dot(dBL, dC);
    viewerFrustumCorners[Frustum::Corner_Left_Top_Far] = viewerFrustumCorners[Frustum::Corner_Left_Top_Near] + dTL * farDistance * scaleTL;
    viewerFrustumCorners[Frustum::Corner_Right_Top_Far] = viewerFrustumCorners[Frustum::Corner_Right_Top_Near] + dTR * farDistance * scaleTR;
    viewerFrustumCorners[Frustum::Corner_Right_Bottom_Far] = viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] + dBR * farDistance * scaleBR;
    viewerFrustumCorners[Frustum::Corner_Left_Bottom_Far] = viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] + dBL * farDistance * scaleBL;
    viewerFrustumCorners[Frustum::Corner_Left_Top_Near] = viewerFrustumCorners[Frustum::Corner_Left_Top_Near] + dTL * nearDistance * scaleTL;
    viewerFrustumCorners[Frustum::Corner_Right_Top_Near] = viewerFrustumCorners[Frustum::Corner_Right_Top_Near] + dTR * nearDistance * scaleTR;
    viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] = viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] + dBR * nearDistance * scaleBR;
    viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] = viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] + dBL * nearDistance * scaleBL;

    // Calculate center of this subsection of the viewer frustum.
    glm::dvec3 worldSpaceFrustumCenter(0.0);
    for (size_t i = 0; i < Frustum::NumCorners; ++i) {
        worldSpaceFrustumCenter += viewerFrustumCorners[i];
    }
    worldSpaceFrustumCenter /= Frustum::NumCorners;

    Transform shadowCameraTransform;
    shadowCameraTransform.setTranslation(worldSpaceFrustumCenter);
    shadowCameraTransform.setRotation(m_testDirectionShadowMap->getDirection(), glm::vec3(0.0F, 1.0F, 0.0F));
    glm::dmat4 shadowViewMatrix = glm::inverse(shadowCameraTransform.getMatrix());

    glm::dvec2 shadowViewMin(std::numeric_limits<double>::max());
    glm::dvec2 shadowViewMax(std::numeric_limits<double>::min());

    const bool useBoundingSphere = false;
    glm::dvec3 lightSpaceFrustumCenter;

    if (useBoundingSphere) {
        lightSpaceFrustumCenter = glm::dvec3(shadowViewMatrix * glm::dvec4(worldSpaceFrustumCenter, 1.0F));
    }

    double boundingSphereRadius = 0.0;

    for (size_t i = 0; i < Frustum::NumCorners; ++i) {
        temp = shadowViewMatrix * glm::dvec4(viewerFrustumCorners[i], 1.0F); // Light-space frustum corner
        if (useBoundingSphere) {
            glm::dvec3 dist = glm::dvec3(temp) - lightSpaceFrustumCenter;
            boundingSphereRadius = glm::max(boundingSphereRadius, glm::dot(dist, dist)); // Use distance^2 until we found the largest, then sqrt it
        } else {
            shadowViewMin.x = glm::min(shadowViewMin.x, temp.x);
            shadowViewMin.y = glm::min(shadowViewMin.y, temp.y);
            shadowViewMax.x = glm::max(shadowViewMax.x, temp.x);
            shadowViewMax.y = glm::max(shadowViewMax.y, temp.y);
        }
    }

    if (useBoundingSphere) {
        boundingSphereRadius = glm::sqrt(boundingSphereRadius);
        shadowViewMin = glm::dvec2(lightSpaceFrustumCenter.x - boundingSphereRadius, lightSpaceFrustumCenter.y - boundingSphereRadius);
        shadowViewMax = glm::dvec2(lightSpaceFrustumCenter.x + boundingSphereRadius, lightSpaceFrustumCenter.y + boundingSphereRadius);
    }

    Camera shadowCameraProjection(shadowViewMin.x, shadowViewMax.x, shadowViewMin.y, shadowViewMax.y, -64.0, +64.0, true);

    outShadowRenderCamera->setProjection(shadowCameraProjection);
    outShadowRenderCamera->setTransform(shadowCameraTransform);
    outShadowRenderCamera->update();
}