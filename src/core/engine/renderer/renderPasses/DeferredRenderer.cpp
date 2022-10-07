#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "LightRenderer.h"
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/geometry/MeshData.h"
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

#define UNIFORM_BUFFER_BINDING 0
#define ALBEDO_TEXTURE_BINDING 1
#define NORMAL_TEXTURE_BINDING 2
#define EMISSION_TEXTURE_BINDING 3
#define DEPTH_TEXTURE_BINDING 4
#define ENVIRONMENT_CUBEMAP_BINDING 5
#define SPECULAR_REFLECTION_CUBEMAP_BINDING 6
#define DIFFUSE_IRRADIANCE_CUBEMAP_BINDING 7
#define BRDF_INTEGRATION_MAP_BINDING 8


EnvironmentMap* environmentMap = nullptr;


DeferredGeometryRenderPass::DeferredGeometryRenderPass() {

}

DeferredGeometryRenderPass::~DeferredGeometryRenderPass() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        for (size_t j = 0; j < NumAttachments; ++j) {
            delete m_resources[i]->imageViews[j];
            delete m_resources[i]->images[j];
        }
        delete m_resources[i]->cameraInfoBuffer;
        delete m_resources[i]->globalDescriptorSet;
        delete m_resources[i]->framebuffer;
    }
    Engine::eventDispatcher()->disconnect(&DeferredGeometryRenderPass::recreateSwapchain, this);
}

bool DeferredGeometryRenderPass::init() {
    m_graphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();
    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    m_globalDescriptorSetLayout = builder
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex)
            .build();

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->globalDescriptorSet = DescriptorSet::create(m_globalDescriptorSetLayout, descriptorPool);
        m_resources[i]->imageViews = {}; // Default initialize elements to nullptr
        m_resources[i]->images = {};
        m_resources[i]->framebuffer = nullptr;

        BufferConfiguration cameraInfoBufferConfig;
        cameraInfoBufferConfig.device = Engine::graphics()->getDevice();
        cameraInfoBufferConfig.size = sizeof(GPUCamera);
        cameraInfoBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        cameraInfoBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        m_resources[i]->cameraInfoBuffer = Buffer::create(cameraInfoBufferConfig);

        DescriptorSetWriter(m_resources[i]->globalDescriptorSet)
                .writeBuffer(0, m_resources[i]->cameraInfoBuffer, 0, m_resources[i]->cameraInfoBuffer->getSize())
                .write();
    }

    if (!createRenderPass()) {
        printf("Failed to create DeferredRenderer RenderPass\n");
        return false;
    }

    Engine::eventDispatcher()->connect(&DeferredGeometryRenderPass::recreateSwapchain, this);
    return true;
}

void DeferredGeometryRenderPass::render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("DeferredGeometryRenderPass::render");

    renderCamera->uploadCameraData(m_resources->cameraInfoBuffer, 0);

    PROFILE_REGION("Bind resources");

    std::vector<vk::DescriptorSet> descriptorSets = {
            m_resources->globalDescriptorSet->getDescriptorSet(),
            Engine::sceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
            Engine::sceneRenderer()->getMaterialDescriptorSet()->getDescriptorSet(),
    };

    std::vector<uint32_t> dynamicOffsets = {};

    GraphicsPipeline* graphicsPipeline = Engine::deferredGeometryPass()->getGraphicsPipeline();
    graphicsPipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

    Engine::sceneRenderer()->render(dt, commandBuffer, renderCamera);
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

ImageView* DeferredGeometryRenderPass::getAlbedoImageView() const {
    return m_resources->imageViews[Attachment_AlbedoRGB_Roughness];
}

ImageView* DeferredGeometryRenderPass::getNormalImageView() const {
    return m_resources->imageViews[Attachment_NormalXYZ_Metallic];
}

ImageView* DeferredGeometryRenderPass::getEmissionImageView() const {
    return m_resources->imageViews[Attachment_EmissionRGB_AO];
}

ImageView* DeferredGeometryRenderPass::getDepthImageView() const {
    return m_resources->imageViews[Attachment_Depth];
}

vk::Format DeferredGeometryRenderPass::getAttachmentFormat(const uint32_t& attachment) const {
    switch (attachment) {
        case Attachment_AlbedoRGB_Roughness: return vk::Format::eR8G8B8A8Unorm;
        case Attachment_NormalXYZ_Metallic: return vk::Format::eR16G16B16A16Sfloat;
        case Attachment_EmissionRGB_AO: return vk::Format::eR16G16B16A16Unorm;
        case Attachment_Depth: return Engine::graphics()->getDepthFormat();
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
    setResolution(Engine::graphics()->getResolution());
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
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.sampleCount = sampleCount;
    imageConfig.setSize(m_resolution);

    ImageViewConfiguration imageViewConfig;
    imageViewConfig.device = Engine::graphics()->getDevice();

    // Albedo/Roughness image
    imageConfig.format = getAttachmentFormat(Attachment_AlbedoRGB_Roughness);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->images[Attachment_AlbedoRGB_Roughness] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_AlbedoRGB_Roughness);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resources->images[Attachment_AlbedoRGB_Roughness]);
    resources->imageViews[Attachment_AlbedoRGB_Roughness] = ImageView::create(imageViewConfig);

    // Normal/Metallic image
    imageConfig.format = getAttachmentFormat(Attachment_NormalXYZ_Metallic);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->images[Attachment_NormalXYZ_Metallic] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_NormalXYZ_Metallic);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resources->images[Attachment_NormalXYZ_Metallic]);
    resources->imageViews[Attachment_NormalXYZ_Metallic] = ImageView::create(imageViewConfig);

    // Emission/AmbientOcclusion image
    imageConfig.format = getAttachmentFormat(Attachment_EmissionRGB_AO);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    resources->images[Attachment_EmissionRGB_AO] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_EmissionRGB_AO);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageViewConfig.setImage(resources->images[Attachment_EmissionRGB_AO]);
    resources->imageViews[Attachment_EmissionRGB_AO] = ImageView::create(imageViewConfig);

    // Depth image
    imageConfig.format = getAttachmentFormat(Attachment_Depth);
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment;
    resources->images[Attachment_Depth] = Image2D::create(imageConfig);
    imageViewConfig.format = getAttachmentFormat(Attachment_Depth);
    imageViewConfig.aspectMask = vk::ImageAspectFlagBits::eDepth;
//    if (ImageUtil::isStencilAttachment(imageViewConfig.format))
//        imageViewConfig.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    imageViewConfig.setImage(resources->images[Attachment_Depth]);
    resources->imageViews[Attachment_Depth] = ImageView::create(imageViewConfig);

    // Framebuffer
    FramebufferConfiguration framebufferConfig;
    framebufferConfig.device = Engine::graphics()->getDevice();
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
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_renderPass;
    pipelineConfig.setViewport(m_resolution);
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
    return m_graphicsPipeline->recreate(pipelineConfig);
}

bool DeferredGeometryRenderPass::createRenderPass() {
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

    attachments[Attachment_Depth].setFormat(getAttachmentFormat(Attachment_Depth));
    attachments[Attachment_Depth].setSamples(samples);
    attachments[Attachment_Depth].setLoadOp(vk::AttachmentLoadOp::eClear);
    attachments[Attachment_Depth].setStoreOp(vk::AttachmentStoreOp::eStore); // could be eDontCare if we don't need to sample the depth buffer
    attachments[Attachment_Depth].setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    attachments[Attachment_Depth].setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    attachments[Attachment_Depth].setInitialLayout(vk::ImageLayout::eUndefined);
    attachments[Attachment_Depth].setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal); // eDepthStencilAttachmentOptimal if we don't need to sample the depth buffer

    SubpassConfiguration subpassConfiguration;
    subpassConfiguration.addColourAttachment(Attachment_AlbedoRGB_Roughness, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.addColourAttachment(Attachment_NormalXYZ_Metallic, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.addColourAttachment(Attachment_EmissionRGB_AO, vk::ImageLayout::eColorAttachmentOptimal);
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
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.setSubpassDependencies(dependencies);
    renderPassConfig.setClearColour(Attachment_AlbedoRGB_Roughness, glm::vec4(0.0F, 0.25F, 0.5F, 1.0F));
    renderPassConfig.setClearColour(Attachment_NormalXYZ_Metallic, glm::vec4(0.0F, 0.0F, 0.0F, 0.0F));
    renderPassConfig.setClearColour(Attachment_EmissionRGB_AO, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));
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
        delete m_resources[i]->lightingDescriptorSet;
        delete m_resources[i]->uniformBuffer;
    }
    for (size_t i = 0; i < NumAttachments; ++i)
        delete m_attachmentSamplers[i];

    delete environmentMap;

    Engine::eventDispatcher()->disconnect(&DeferredLightingRenderPass::recreateSwapchain, this);
}

bool DeferredLightingRenderPass::init() {
    m_graphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(Engine::graphics()->getDevice()));

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    m_lightingDescriptorSetLayout = DescriptorSetLayoutBuilder(descriptorPool->getDevice())
            .addUniformBuffer(UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(ALBEDO_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(NORMAL_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(EMISSION_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(DEPTH_TEXTURE_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(ENVIRONMENT_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(SPECULAR_REFLECTION_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(DIFFUSE_IRRADIANCE_CUBEMAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(BRDF_INTEGRATION_MAP_BINDING, vk::ShaderStageFlagBits::eFragment)
            .build();

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());

        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        bufferConfig.size = sizeof(LightingPassUniformData);
        m_resources[i]->uniformBuffer = Buffer::create(bufferConfig);

        m_resources[i]->lightingDescriptorSet = DescriptorSet::create(m_lightingDescriptorSetLayout, descriptorPool);

        DescriptorSetWriter(m_resources[i]->lightingDescriptorSet)
                .writeBuffer(UNIFORM_BUFFER_BINDING, m_resources[i]->uniformBuffer, 0, m_resources[i]->uniformBuffer->getSize())
                .write();
    }

    ImageData* defaultEnvironmentCubeMap = new ImageData(1, 1, ImagePixelLayout::RGBA, ImagePixelFormat::Float32);
    for (size_t y = 0; y < defaultEnvironmentCubeMap->getHeight(); ++y) {
        for (size_t x = 0; x < defaultEnvironmentCubeMap->getWidth(); ++x) {
            defaultEnvironmentCubeMap->setPixelf(x, y, 0.4F, 0.53F, 0.74F, 1.0F);
        }
    }

    ImageCubeConfiguration imageCubeConfig;
    imageCubeConfig.device = Engine::graphics()->getDevice();
    imageCubeConfig.format = vk::Format::eR32G32B32A32Sfloat;
    imageCubeConfig.usage = vk::ImageUsageFlagBits::eSampled;
    imageCubeConfig.generateMipmap = true;
    imageCubeConfig.mipLevels = UINT32_MAX;
    imageCubeConfig.imageSource.setEquirectangularSource("res/environment_maps/wide_street_02_8k.hdr");
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_PosX, defaultEnvironmentCubeMap);
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_NegX, defaultEnvironmentCubeMap);
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_PosY, defaultEnvironmentCubeMap);
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_NegY, defaultEnvironmentCubeMap);
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_PosZ, defaultEnvironmentCubeMap);
//    imageCubeConfig.imageSource.setFaceSource(ImageCubeFace_NegZ, defaultEnvironmentCubeMap);
    std::shared_ptr<ImageCube> imageCube = std::shared_ptr<ImageCube>(ImageCube::create(imageCubeConfig));


    environmentMap = new EnvironmentMap();
    environmentMap->setEnvironmentImage(imageCube);
    environmentMap->update();

    SamplerConfiguration samplerConfig;
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eNearest;
    samplerConfig.magFilter = vk::Filter::eNearest;
    samplerConfig.wrapU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerConfig.wrapV = vk::SamplerAddressMode::eMirroredRepeat;

    m_attachmentSamplers[Attachment_AlbedoRGB_Roughness] = Sampler::create(samplerConfig);
    m_attachmentSamplers[Attachment_NormalXYZ_Metallic] = Sampler::create(samplerConfig);
    m_attachmentSamplers[Attachment_EmissionRGB_AO] = Sampler::create(samplerConfig);
    m_attachmentSamplers[Attachment_Depth] = Sampler::create(samplerConfig);

    Engine::eventDispatcher()->connect(&DeferredLightingRenderPass::recreateSwapchain, this);
    return true;
}

void DeferredLightingRenderPass::renderScreen(double dt) {

    const Entity& cameraEntity = Engine::scene()->getMainCameraEntity();
    m_renderCamera.setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera.setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera.update();

    const vk::CommandBuffer& commandBuffer = Engine::graphics()->getCurrentCommandBuffer();
    const Framebuffer* framebuffer = Engine::graphics()->getCurrentFramebuffer();
    const auto& renderPass = Engine::graphics()->renderPass();
    renderPass->begin(commandBuffer, framebuffer, vk::SubpassContents::eInline);

    m_graphicsPipeline->bind(commandBuffer);

    DescriptorSetWriter(m_resources->lightingDescriptorSet)
        .writeImage(ALBEDO_TEXTURE_BINDING, m_attachmentSamplers[Attachment_AlbedoRGB_Roughness], m_geometryPass->getAlbedoImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(NORMAL_TEXTURE_BINDING, m_attachmentSamplers[Attachment_NormalXYZ_Metallic], m_geometryPass->getNormalImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(EMISSION_TEXTURE_BINDING, m_attachmentSamplers[Attachment_EmissionRGB_AO], m_geometryPass->getEmissionImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(DEPTH_TEXTURE_BINDING, m_attachmentSamplers[Attachment_Depth], m_geometryPass->getDepthImageView(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(ENVIRONMENT_CUBEMAP_BINDING, environmentMap->getEnvironmentMapTexture().get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(SPECULAR_REFLECTION_CUBEMAP_BINDING, environmentMap->getSpecularReflectionMapTexture().get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(DIFFUSE_IRRADIANCE_CUBEMAP_BINDING, environmentMap->getDiffuseIrradianceMapTexture().get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .writeImage(BRDF_INTEGRATION_MAP_BINDING, EnvironmentMap::getBRDFIntegrationMap().get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
        .write();

    vk::DescriptorSet descriptorSets[2] = {
            m_resources->lightingDescriptorSet->getDescriptorSet(),
            Engine::lightRenderer()->getLightingRenderPassDescriptorSet()->getDescriptorSet(),
    };

    glm::mat4x4 projectedRays = m_renderCamera.getInverseProjectionMatrix() * glm::mat4(
            -1, +1, 0, 1,
            +1, +1, 0, 1,
            +1, -1, 0, 1,
            -1, -1, 0, 1);
    glm::mat4 viewCameraRays = projectedRays / projectedRays[3][3];

    // Ignore translation component of view matrix
    glm::mat4 worldCameraRays = glm::mat4(glm::mat3(m_renderCamera.getInverseViewMatrix())) * viewCameraRays;

    LightingPassUniformData uniformData{};
    uniformData.viewMatrix = m_renderCamera.getViewMatrix();
    uniformData.projectionMatrix = m_renderCamera.getProjectionMatrix();
    uniformData.viewProjectionMatrix = m_renderCamera.getViewProjectionMatrix();
    uniformData.invViewMatrix = m_renderCamera.getInverseViewMatrix();
    uniformData.invProjectionMatrix = m_renderCamera.getInverseProjectionMatrix();
    uniformData.invViewProjectionMatrix = m_renderCamera.getInverseViewProjectionMatrix();
    uniformData.cameraRays = worldCameraRays;
    m_resources->uniformBuffer->upload(0, sizeof(LightingPassUniformData), &uniformData);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline->getPipelineLayout(), 0, 2, descriptorSets, 0, nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRenderPass();
}

GraphicsPipeline* DeferredLightingRenderPass::getGraphicsPipeline() const {
    return m_graphicsPipeline.get();
}

void DeferredLightingRenderPass::recreateSwapchain(const RecreateSwapchainEvent& event) {
    GraphicsPipelineConfiguration pipelineConfig;
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = Engine::graphics()->renderPass();
    pipelineConfig.setViewport(0, 0); // Default to full window resolution
    pipelineConfig.vertexShader = "res/shaders/screen/fullscreen_quad.vert";
    pipelineConfig.fragmentShader = "res/shaders/deferred/lighting.frag";
//    pipelineConfig.addVertexInputBinding(0, sizeof(glm::vec2), vk::VertexInputRate::eVertex);
//    pipelineConfig.addVertexInputAttribute(0, 0, vk::Format::eR32G32Sfloat, 0); // XY
    pipelineConfig.addDescriptorSetLayout(m_lightingDescriptorSetLayout.get());
    pipelineConfig.addDescriptorSetLayout(Engine::lightRenderer()->getLightingRenderPassDescriptorSetLayout().get());
    m_graphicsPipeline->recreate(pipelineConfig);
}