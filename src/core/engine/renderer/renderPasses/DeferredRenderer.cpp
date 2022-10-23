#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/geometry/MeshData.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/application/Engine.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/Framebuffer.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageCube.h"
#include "core/graphics/Texture.h"
#include "core/graphics/Buffer.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"

#define UNIFORM_BUFFER_BINDING 0
#define ALBEDO_TEXTURE_BINDING 1
#define NORMAL_TEXTURE_BINDING 2
#define EMISSION_TEXTURE_BINDING 3
#define VELOCITY_TEXTURE_BINDING 4
#define DEPTH_TEXTURE_BINDING 5
#define ENVIRONMENT_CUBEMAP_BINDING 6
#define SPECULAR_REFLECTION_CUBEMAP_BINDING 7
#define DIFFUSE_IRRADIANCE_CUBEMAP_BINDING 8
#define BRDF_INTEGRATION_MAP_BINDING 9

EnvironmentMap* environmentMap = nullptr;


DeferredRenderer::DeferredRenderer():
    m_attachmentSampler(nullptr) {
}

DeferredRenderer::~DeferredRenderer() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->globalDescriptorSet;
        delete m_resources[i]->lightingPassDescriptorSet;
        delete m_resources[i]->cameraInfoBuffer;
        delete m_resources[i]->lightingPassUniformBuffer;
        delete m_resources[i]->frame.framebuffer;
        for (size_t j = 0; j < NumAttachments; ++j) {
            delete m_resources[i]->frame.imageViews[j];
            delete m_resources[i]->frame.images[j];
        }
    }

    delete m_attachmentSampler;

    delete environmentMap;

    Engine::eventDispatcher()->disconnect(&DeferredRenderer::recreateSwapchain, this);
}

bool DeferredRenderer::init() {
    m_geometryGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));
    m_lightingGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    m_globalDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .build("DeferredRenderer-GlobalDescriptorSetLayout");

    m_lightingDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addInputAttachment(ALBEDO_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addInputAttachment(NORMAL_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addInputAttachment(EMISSION_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addInputAttachment(VELOCITY_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addInputAttachment(DEPTH_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(ENVIRONMENT_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(SPECULAR_REFLECTION_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(DIFFUSE_IRRADIANCE_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(BRDF_INTEGRATION_MAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build("DeferredRenderer-LightingPassDescriptorSetLayout");

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());

        m_resources[i]->frame.images = {}; // Default initialize elements to nullptr
        m_resources[i]->frame.imageViews = {};

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;

        bufferConfig.size = sizeof(GeometryPassUniformData);
        m_resources[i]->cameraInfoBuffer = Buffer::create(bufferConfig, "DeferredRenderer-CameraInfoBuffer");

        bufferConfig.size = sizeof(LightingPassUniformData);
        m_resources[i]->lightingPassUniformBuffer = Buffer::create(bufferConfig, "DeferredRenderer-LightingPassUniformBuffer");

        m_resources[i]->globalDescriptorSet = DescriptorSet::create(m_globalDescriptorSetLayout, descriptorPool, "DeferredRenderer-GlobalDescriptorSet");
        DescriptorSetWriter(m_resources[i]->globalDescriptorSet)
                .writeBuffer(0, m_resources[i]->cameraInfoBuffer, 0, m_resources[i]->cameraInfoBuffer->getSize())
                .write();

        m_resources[i]->lightingPassDescriptorSet = DescriptorSet::create(m_lightingDescriptorSetLayout, descriptorPool, "DeferredRenderer-LightingPassDescriptorSet");
        DescriptorSetWriter(m_resources[i]->lightingPassDescriptorSet)
                .writeBuffer(UNIFORM_BUFFER_BINDING, m_resources[i]->lightingPassUniformBuffer)
                .write();

        m_resources[i]->updateDescriptorSet = true;
    }

    if (!createRenderPass()) {
        printf("Failed to create DeferredLighting RenderPass\n");
        return false;
    }

    ImageData* defaultEnvironmentCubeMap = new ImageData(1, 1, ImagePixelLayout::RGBA, ImagePixelFormat::Float32);
    for (uint32_t y = 0; y < defaultEnvironmentCubeMap->getHeight(); ++y) {
        for (uint32_t x = 0; x < defaultEnvironmentCubeMap->getWidth(); ++x) {
            defaultEnvironmentCubeMap->setPixelf(x, y, 0.4F, 0.53F, 0.74F, 1.0F);
        }
    }

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eNearest;
    samplerConfig.magFilter = vk::Filter::eNearest;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;
    m_attachmentSampler = Sampler::create(samplerConfig, "DeferredRenderer-GBufferAttachmentSampler");

    ImageCubeConfiguration imageCubeConfig{};
    imageCubeConfig.device = Engine::graphics()->getDevice();
    imageCubeConfig.format = vk::Format::eR32G32B32A32Sfloat;
    imageCubeConfig.usage = vk::ImageUsageFlagBits::eSampled;
    imageCubeConfig.generateMipmap = true;
    imageCubeConfig.mipLevels = UINT32_MAX;
    imageCubeConfig.imageSource.setEquirectangularSource("res/environment_maps/wide_street_02_8k.hdr");
    std::shared_ptr<ImageCube> imageCube = std::shared_ptr<ImageCube>(ImageCube::create(imageCubeConfig, "DeferredRenderer-DefaultSkyboxCubeImage"));

    environmentMap = new EnvironmentMap();
    environmentMap->setEnvironmentImage(imageCube);
    environmentMap->update();

    Engine::eventDispatcher()->connect(&DeferredRenderer::recreateSwapchain, this);
    return true;
}

void DeferredRenderer::preRender(const double& dt) {
//    if (CONCURRENT_FRAMES > 1) {
//        m_prevFrameImage = &m_resources[Engine::graphics()->getPreviousFrameIndex()]->frameImage;
//    } else {
//        swapFrameImage(m_prevFrameImage, &m_resources->frameImage);
//    }
}

void DeferredRenderer::renderGeometryPass(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("DeferredRenderer::renderGeometryPass");
    BEGIN_CMD_LABEL(commandBuffer, "DeferredRenderer::renderGeometryPass");

    GeometryPassUniformData uniformData{};

    uniformData.prevCamera.viewMatrix = renderCamera->getPrevViewMatrix();
    uniformData.prevCamera.projectionMatrix = renderCamera->getPrevProjectionMatrix();
    uniformData.prevCamera.viewProjectionMatrix = renderCamera->getPrevViewProjectionMatrix();
    uniformData.camera.viewMatrix = renderCamera->getViewMatrix();
    uniformData.camera.projectionMatrix = renderCamera->getProjectionMatrix();
    uniformData.camera.viewProjectionMatrix = renderCamera->getViewProjectionMatrix();

    if (Engine::reprojectionRenderer()->isTaaEnabled()) {
        uniformData.taaPreviousJitterOffset = Engine::reprojectionRenderer()->getTaaPreviousJitterOffset();
        uniformData.taaCurrentJitterOffset = Engine::reprojectionRenderer()->getTaaCurrentJitterOffset();
        glm::mat4 previousJitterOffsetMatrix = glm::translate(glm::mat4(1.0F),glm::vec3(uniformData.taaPreviousJitterOffset, 0.0F));
        glm::mat4 currentJitterOffsetMatrix = glm::translate(glm::mat4(1.0F),glm::vec3(uniformData.taaCurrentJitterOffset, 0.0F));
        uniformData.prevCamera.projectionMatrix = previousJitterOffsetMatrix * uniformData.prevCamera.projectionMatrix;
        uniformData.prevCamera.viewProjectionMatrix = previousJitterOffsetMatrix * uniformData.prevCamera.viewProjectionMatrix;
        uniformData.camera.projectionMatrix = currentJitterOffsetMatrix * uniformData.camera.projectionMatrix;
        uniformData.camera.viewProjectionMatrix = currentJitterOffsetMatrix * uniformData.camera.viewProjectionMatrix;
    } else {
        uniformData.taaPreviousJitterOffset = glm::vec2(0.0F);
        uniformData.taaCurrentJitterOffset = glm::vec2(0.0F);
    }

    m_resources->cameraInfoBuffer->upload(0, sizeof(GeometryPassUniformData), &uniformData);
//    renderCamera->uploadCameraData(m_resources->cameraInfoBuffer, 0);


    std::array<vk::DescriptorSet, 3> descriptorSets = {
            m_resources->globalDescriptorSet->getDescriptorSet(),
            Engine::sceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
            Engine::sceneRenderer()->getMaterialDescriptorSet()->getDescriptorSet(),
    };

    std::array<uint32_t, 0> dynamicOffsets = {}; // zero-size array okay?

    m_geometryGraphicsPipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_geometryGraphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

    Engine::sceneRenderer()->render(dt, commandBuffer, renderCamera);
    END_CMD_LABEL(commandBuffer);
}

void DeferredRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("DeferredRenderer::render")
    const Entity& cameraEntity = Engine::scene()->getMainCameraEntity();
    m_renderCamera.setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera.setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera.update();

    glm::mat4x4 projectedRays = m_renderCamera.getInverseProjectionMatrix() * glm::mat4(
            -1, +1, 0, 1,
            +1, +1, 0, 1,
            +1, -1, 0, 1,
            -1, -1, 0, 1);
    glm::mat4 viewCameraRays = projectedRays / projectedRays[3][3];
    glm::mat4 worldCameraRays = glm::mat4(glm::mat3(m_renderCamera.getInverseViewMatrix())) * viewCameraRays; // we ignore the translation component of the view matrix

    LightingPassUniformData uniformData{};
    uniformData.viewMatrix = m_renderCamera.getViewMatrix();
    uniformData.projectionMatrix = m_renderCamera.getProjectionMatrix();
    uniformData.viewProjectionMatrix = m_renderCamera.getViewProjectionMatrix();
    uniformData.invViewMatrix = m_renderCamera.getInverseViewMatrix();
    uniformData.invProjectionMatrix = m_renderCamera.getInverseProjectionMatrix();
    uniformData.invViewProjectionMatrix = m_renderCamera.getInverseViewProjectionMatrix();
    uniformData.resolution = glm::uvec2(Engine::graphics()->getResolution());
    uniformData.cameraRays = worldCameraRays;
    uniformData.showDebugShadowCascades = false;
    uniformData.debugShadowCascadeLightIndex = 0;
    uniformData.debugShadowCascadeOpacity = 0.5F;

    DescriptorSetWriter descriptorSetWriter(m_resources->lightingPassDescriptorSet);

    if (m_resources->updateDescriptorSet) {
        m_resources->updateDescriptorSet = false;
        descriptorSetWriter.writeImage(ALBEDO_TEXTURE_BINDING, m_attachmentSampler, getAlbedoImageView(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(NORMAL_TEXTURE_BINDING, m_attachmentSampler, getNormalImageView(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(EMISSION_TEXTURE_BINDING, m_attachmentSampler, getEmissionImageView(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(VELOCITY_TEXTURE_BINDING, m_attachmentSampler, getVelocityImageView(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(DEPTH_TEXTURE_BINDING, m_attachmentSampler, getDepthImageView(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(ENVIRONMENT_CUBEMAP_BINDING, environmentMap->getEnvironmentMapTexture().get(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(SPECULAR_REFLECTION_CUBEMAP_BINDING,environmentMap->getSpecularReflectionMapTexture().get(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(DIFFUSE_IRRADIANCE_CUBEMAP_BINDING, environmentMap->getDiffuseIrradianceMapTexture().get(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
        descriptorSetWriter.writeImage(BRDF_INTEGRATION_MAP_BINDING, EnvironmentMap::getBRDFIntegrationMap(commandBuffer).get(),vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1);
    }
    descriptorSetWriter.write();

    BEGIN_CMD_LABEL(commandBuffer, "DeferredRenderer::render");

    m_lightingGraphicsPipeline->bind(commandBuffer);

    std::array<vk::DescriptorSet, 2> descriptorSets = {
            m_resources->lightingPassDescriptorSet->getDescriptorSet(),
            Engine::lightRenderer()->getLightingRenderPassDescriptorSet()->getDescriptorSet(),
    };

    m_resources->lightingPassUniformBuffer->upload(0, sizeof(LightingPassUniformData), &uniformData);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_lightingGraphicsPipeline->getPipelineLayout(), 0, descriptorSets, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    END_CMD_LABEL(commandBuffer);

    m_resources->frame.rendered = true;
    swapFrame();
}

void DeferredRenderer::presentDirect(const vk::CommandBuffer& commandBuffer) {
//    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
//    ImageUtil::transitionLayout(commandBuffer, m_resources->frameImage.image->getImage(), subresourceRange, ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader), ImageTransition::TransferSrc());
//
//    Engine::graphics()->presentImageDirect(commandBuffer, m_resources->frameImage.image->getImage(), vk::ImageLayout::eTransferSrcOptimal);
//
//    ImageUtil::transitionLayout(commandBuffer, m_resources->frameImage.image->getImage(), subresourceRange, ImageTransition::TransferSrc(), ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader));
}

bool DeferredRenderer::hasPreviousFrame() const {
    return m_previousFrame.rendered;
}

void DeferredRenderer::beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    m_renderPass->begin(commandBuffer, m_resources->frame.framebuffer, subpassContents);
}

void DeferredRenderer::beginGeometrySubpass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
//    m_geometryRenderPass->begin(commandBuffer, m_resources->geometryFramebuffer, subpassContents);
}

void DeferredRenderer::beginLightingSubpass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents) {
    commandBuffer.nextSubpass(vk::SubpassContents::eInline);
//    m_lightingRenderPass->begin(commandBuffer, m_resources->frameImage.framebuffer, subpassContents);
}

ImageView* DeferredRenderer::getAlbedoImageView() const {
    return m_resources->frame.imageViews[Attachment_AlbedoRGB_Roughness];
}

ImageView* DeferredRenderer::getNormalImageView() const {
    return m_resources->frame.imageViews[Attachment_NormalXYZ_Metallic];
}

ImageView* DeferredRenderer::getEmissionImageView() const {
    return m_resources->frame.imageViews[Attachment_EmissionRGB_AO];
}

ImageView* DeferredRenderer::getVelocityImageView() const {
    return m_resources->frame.imageViews[Attachment_VelocityXY];
}

ImageView* DeferredRenderer::getDepthImageView() const {
    return m_resources->frame.imageViews[Attachment_Depth];
}

ImageView* DeferredRenderer::getOutputFrameImageView() const {
    return m_resources->frame.imageViews[Attachment_LightingRGB];
}

ImageView* DeferredRenderer::getPreviousAlbedoImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_AlbedoRGB_Roughness] : getAlbedoImageView();
}

ImageView* DeferredRenderer::getPreviousNormalImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_NormalXYZ_Metallic] : getNormalImageView();
}

ImageView* DeferredRenderer::getPreviousEmissionImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_EmissionRGB_AO] : getEmissionImageView();
}

ImageView* DeferredRenderer::getPreviousVelocityImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_VelocityXY] : getVelocityImageView();
}

ImageView* DeferredRenderer::getPreviousDepthImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_Depth] : getDepthImageView();
}

ImageView* DeferredRenderer::getPreviousOutputFrameImageView() const {
    return hasPreviousFrame() ? m_previousFrame.imageViews[Attachment_LightingRGB] : getOutputFrameImageView();
}

vk::Format DeferredRenderer::getAttachmentFormat(const uint32_t& attachment) const {
    switch (attachment) {
        case Attachment_AlbedoRGB_Roughness: return vk::Format::eR8G8B8A8Unorm;
        case Attachment_NormalXYZ_Metallic: return vk::Format::eR16G16B16A16Sfloat;
        case Attachment_EmissionRGB_AO: return vk::Format::eR16G16B16A16Unorm;
        case Attachment_VelocityXY: return vk::Format::eR16G16B16A16Sfloat;
        case Attachment_Depth: return Engine::graphics()->getDepthFormat();
        case Attachment_LightingRGB: return getOutputColourFormat();
        default:
            assert(false);
            return vk::Format::eUndefined;
    }
}

vk::Format DeferredRenderer::getOutputColourFormat() const {
    return vk::Format::eR16G16B16A16Sfloat;
//    return vk::Format::eR32G32B32A32Sfloat;
}

void DeferredRenderer::recreateSwapchain(RecreateSwapchainEvent* event) {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources[i]->updateDescriptorSet = true;
        createFramebuffer(&m_resources[i]->frame);
    }

    if (CONCURRENT_FRAMES == 1) {
        createFramebuffer(&m_previousFrame);
    } else {
        m_previousFrame.images = {};
        m_previousFrame.imageViews = {};
        m_previousFrame.framebuffer = nullptr;
        m_previousFrame.rendered = false;
    }

    createGeometryGraphicsPipeline();
    createLightingGraphicsPipeline();
    m_frameIndices.clear();
}

bool DeferredRenderer::createFramebuffer(FrameImages* frame) {
    delete frame->framebuffer;
    for (const auto& imageView: frame->imageViews)
        delete imageView;
    for (const auto& image: frame->images)
        delete image;

    frame->rendered = false;

    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = sampleCount;
    imageConfig.setSize(Engine::graphics()->getResolution());

    ImageViewConfiguration imageViewConfig{};
    imageViewConfig.device = Engine::graphics()->getDevice();

    // Albedo/Roughness image
    imageConfig.format = getAttachmentFormat(Attachment_AlbedoRGB_Roughness);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment;
    frame->images[Attachment_AlbedoRGB_Roughness] = Image2D::create(imageConfig, "DeferredRenderer-GBufferAlbedoRoughnessImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->images[Attachment_AlbedoRGB_Roughness]);
    frame->imageViews[Attachment_AlbedoRGB_Roughness] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferAlbedoRoughnessImageView");

    // Normal/Metallic image
    imageConfig.format = getAttachmentFormat(Attachment_NormalXYZ_Metallic);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment;
    frame->images[Attachment_NormalXYZ_Metallic] = Image2D::create(imageConfig, "DeferredRenderer-GBufferNormalMetallicImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->images[Attachment_NormalXYZ_Metallic]);
    frame->imageViews[Attachment_NormalXYZ_Metallic] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferNormalMetallicImageView");

    // Emission/AmbientOcclusion image
    imageConfig.format = getAttachmentFormat(Attachment_EmissionRGB_AO);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment;
    frame->images[Attachment_EmissionRGB_AO] = Image2D::create(imageConfig, "DeferredRenderer-GBufferEmissionAOImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->images[Attachment_EmissionRGB_AO]);
    frame->imageViews[Attachment_EmissionRGB_AO] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferEmissionAOImageView");

    // Velocity image
    imageConfig.format = getAttachmentFormat(Attachment_VelocityXY);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment;
    frame->images[Attachment_VelocityXY] = Image2D::create(imageConfig, "DeferredRenderer-GBufferVelocityXYImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->images[Attachment_VelocityXY]);
    frame->imageViews[Attachment_VelocityXY] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferVelocityXYImageView");

    // Depth image
    imageConfig.format = getAttachmentFormat(Attachment_Depth);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment;
    frame->images[Attachment_Depth] = Image2D::create(imageConfig, "DeferredRenderer-GBufferDepthImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
//    if (ImageUtil::isStencilAttachment(imageViewConfig.format))
//        imageViewConfig.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    imageViewConfig.setImage(frame->images[Attachment_Depth]);
    frame->imageViews[Attachment_Depth] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferDepthImageView");
    // Lighting output image
    imageConfig.format = getAttachmentFormat(Attachment_LightingRGB);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    frame->images[Attachment_LightingRGB] = Image2D::create(imageConfig, "DeferredRenderer-GBufferVelocityXYImage");
    imageViewConfig.format = imageConfig.format;
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(frame->images[Attachment_LightingRGB]);
    frame->imageViews[Attachment_LightingRGB] = ImageView::create(imageViewConfig, "DeferredRenderer-GBufferVelocityXYImageView");

    // Framebuffer
    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = Engine::graphics()->getDevice();
    framebufferConfig.setSize(Engine::graphics()->getResolution());
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.setAttachments(frame->imageViews);

    frame->framebuffer = Framebuffer::create(framebufferConfig, "DeferredRenderer-GBufferFramebuffer");
    if (frame->framebuffer == nullptr)
        return false;
    return true;
}

bool DeferredRenderer::createGeometryGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.subpass = 0;
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
    pipelineConfig.vertexShader = "res/shaders/main.vert";
    pipelineConfig.fragmentShader = "res/shaders/main.frag";
    pipelineConfig.vertexInputBindings = MeshUtils::getVertexBindingDescriptions<Vertex>();
    pipelineConfig.vertexInputAttributes = MeshUtils::getVertexAttributeDescriptions<Vertex>();
    pipelineConfig.setAttachmentBlendState(0, AttachmentBlendState(false, 0b1111));
    pipelineConfig.setAttachmentBlendState(1, AttachmentBlendState(false, 0b1111));
    pipelineConfig.addDescriptorSetLayout(m_globalDescriptorSetLayout->getDescriptorSetLayout());
    pipelineConfig.addDescriptorSetLayout(Engine::sceneRenderer()->getObjectDescriptorSetLayout()->getDescriptorSetLayout());
    pipelineConfig.addDescriptorSetLayout(Engine::sceneRenderer()->getMaterialDescriptorSetLayout()->getDescriptorSetLayout());
    //pipelineConfig.polygonMode = vk::PolygonMode::eLine;
    return m_geometryGraphicsPipeline->recreate(pipelineConfig, "DeferredRenderer-GraphicsPipeline");
}

bool DeferredRenderer::createLightingGraphicsPipeline() {
    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.subpass = 1;
    pipelineConfig.setViewport(Engine::graphics()->getResolution());
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/deferred/lighting.frag";
    pipelineConfig.addDescriptorSetLayout(m_lightingDescriptorSetLayout.get());
    pipelineConfig.addDescriptorSetLayout(Engine::lightRenderer()->getLightingRenderPassDescriptorSetLayout().get());
    return m_lightingGraphicsPipeline->recreate(pipelineConfig, "DeferredRenderer-LightingGraphicsPipeline");
}

bool DeferredRenderer::createRenderPass() {
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    std::array<vk::AttachmentDescription, NumAttachments> attachments;

    attachments[Attachment_AlbedoRGB_Roughness].setFormat(getAttachmentFormat(Attachment_AlbedoRGB_Roughness));
    attachments[Attachment_AlbedoRGB_Roughness].setSamples(samples);
    attachments[Attachment_AlbedoRGB_Roughness].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_AlbedoRGB_Roughness].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_AlbedoRGB_Roughness].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_AlbedoRGB_Roughness].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_AlbedoRGB_Roughness].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_AlbedoRGB_Roughness].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_NormalXYZ_Metallic].setFormat(getAttachmentFormat(Attachment_NormalXYZ_Metallic));
    attachments[Attachment_NormalXYZ_Metallic].setSamples(samples);
    attachments[Attachment_NormalXYZ_Metallic].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_NormalXYZ_Metallic].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_NormalXYZ_Metallic].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_NormalXYZ_Metallic].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_NormalXYZ_Metallic].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_NormalXYZ_Metallic].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_EmissionRGB_AO].setFormat(getAttachmentFormat(Attachment_EmissionRGB_AO));
    attachments[Attachment_EmissionRGB_AO].setSamples(samples);
    attachments[Attachment_EmissionRGB_AO].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_EmissionRGB_AO].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_EmissionRGB_AO].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_EmissionRGB_AO].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_EmissionRGB_AO].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_EmissionRGB_AO].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_VelocityXY].setFormat(getAttachmentFormat(Attachment_VelocityXY));
    attachments[Attachment_VelocityXY].setSamples(samples);
    attachments[Attachment_VelocityXY].setLoadOp(vk::AttachmentLoadOp::eClear); // could be eDontCare ?
    attachments[Attachment_VelocityXY].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_VelocityXY].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_VelocityXY].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_VelocityXY].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_VelocityXY].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    attachments[Attachment_Depth].setFormat(getAttachmentFormat(Attachment_Depth));
    attachments[Attachment_Depth].setSamples(samples);
    attachments[Attachment_Depth].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[Attachment_Depth].setStoreOp(vk::AttachmentStoreOp::eStore); // could be eDontCare ?
    attachments[Attachment_Depth].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_Depth].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_Depth].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_Depth].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal); // eDepthStencilAttachmentOptimal if we don't need to sample the depth buffer

    attachments[Attachment_LightingRGB].setFormat(getOutputColourFormat());
    attachments[Attachment_LightingRGB].setSamples(samples);
    attachments[Attachment_LightingRGB].setLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_LightingRGB].setStoreOp(vk::AttachmentStoreOp::eStore);
    attachments[Attachment_LightingRGB].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_LightingRGB].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_LightingRGB].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_LightingRGB].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<SubpassConfiguration, 2> subpassConfigurations;
    subpassConfigurations[0].addColourAttachment(Attachment_AlbedoRGB_Roughness, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfigurations[0].addColourAttachment(Attachment_NormalXYZ_Metallic, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfigurations[0].addColourAttachment(Attachment_EmissionRGB_AO, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfigurations[0].addColourAttachment(Attachment_VelocityXY, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfigurations[0].setDepthStencilAttachment(Attachment_Depth, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    subpassConfigurations[1].addInputAttachment(Attachment_AlbedoRGB_Roughness, vk::ImageLayout::eShaderReadOnlyOptimal);
    subpassConfigurations[1].addInputAttachment(Attachment_NormalXYZ_Metallic, vk::ImageLayout::eShaderReadOnlyOptimal);
    subpassConfigurations[1].addInputAttachment(Attachment_EmissionRGB_AO, vk::ImageLayout::eShaderReadOnlyOptimal);
    subpassConfigurations[1].addInputAttachment(Attachment_VelocityXY, vk::ImageLayout::eShaderReadOnlyOptimal);
    subpassConfigurations[1].addInputAttachment(Attachment_Depth, vk::ImageLayout::eShaderReadOnlyOptimal);
    subpassConfigurations[1].addColourAttachment(Attachment_LightingRGB, vk::ImageLayout::eColorAttachmentOptimal);

    std::array<vk::SubpassDependency, 3> dependencies;
    dependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[0].setDstSubpass(0);
    dependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[0].setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[0].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[1].setSrcSubpass(0);
    dependencies[1].setDstSubpass(1);
    dependencies[1].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[1].setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    dependencies[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    dependencies[1].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    dependencies[2].setSrcSubpass(1);
    dependencies[2].setDstSubpass(VK_SUBPASS_EXTERNAL);
    dependencies[2].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependencies[2].setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe);
    dependencies[2].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);
    dependencies[2].setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
    dependencies[2].setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.setSubpasses(subpassConfigurations);
    renderPassConfig.setSubpassDependencies(dependencies);
    renderPassConfig.setClearColour(Attachment_AlbedoRGB_Roughness, glm::vec4(0.0F, 0.25F, 0.5F, 1.0F));
    renderPassConfig.setClearColour(Attachment_NormalXYZ_Metallic, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));
    renderPassConfig.setClearColour(Attachment_EmissionRGB_AO, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));
    renderPassConfig.setClearColour(Attachment_VelocityXY, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));
    renderPassConfig.setClearDepth(Attachment_Depth, 1.0F);
    renderPassConfig.setClearStencil(Attachment_Depth, 0);
    renderPassConfig.setClearColour(Attachment_LightingRGB, glm::vec4(0.0F, 0.0F, 1.0F, 0.0F));

    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig, "DeferredRenderer-GBufferRenderPass"));
    return (bool)m_renderPass;
}


void DeferredRenderer::swapFrame() {
    if (CONCURRENT_FRAMES == 1) {
        std::swap(m_previousFrame.framebuffer, m_resources->frame.framebuffer);
        std::swap(m_previousFrame.imageViews, m_resources->frame.imageViews);
        std::swap(m_previousFrame.images, m_resources->frame.images);
        std::swap(m_previousFrame.rendered, m_resources->frame.rendered);
    } else {
        m_previousFrame.images = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.images;
        m_previousFrame.imageViews = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.imageViews;
        m_previousFrame.framebuffer = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.framebuffer;
        m_previousFrame.rendered = m_resources[Engine::graphics()->getPreviousFrameIndex()]->frame.rendered;
    }
}
