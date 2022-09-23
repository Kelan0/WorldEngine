
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
#include "core/engine/renderer/LightComponent.h"

#define GAUSSIAN_BLUE_DIRECTION_X 0
#define GAUSSIAN_BLUE_DIRECTION_Y 1

#define LIGHTING_RENDER_PASS_UNIFORM_BUFFER_BINDING 0
#define LIGHTING_RENDER_PASS_LIGHT_INFO_BUFFER_BINDING 1
#define LIGHTING_RENDER_PASS_SHADOW_MAP_INFO_BUFFER_BINDING 2
#define LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING 3


struct GaussianBlurPushConstants {
    glm::uvec2 srcSize;
    glm::uvec2 dstSize;
    float blurRadius;
    uint32_t blurDirection;
    uint32_t imageIndex;
};


LightRenderer::LightRenderer()
//:
//    m_vsmBlurIntermediateImageView(nullptr),
//    m_vsmBlurIntermediateImage(nullptr)
{
}

LightRenderer::~LightRenderer() {
    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_shadowRenderPassResources[i]->descriptorSet;
        delete m_shadowRenderPassResources[i]->cameraInfoBuffer;
        delete m_lightingRenderPassResources[i]->descriptorSet;
        delete m_lightingRenderPassResources[i]->lightInfoBuffer;
        delete m_lightingRenderPassResources[i]->shadowMapBuffer;
        delete m_lightingRenderPassResources[i]->uniformBuffer;
    }

    for (auto& shadowMap: m_inactiveShadowMaps)
        delete shadowMap;
    for (auto& entry: m_activeShadowMaps)
        delete entry.first;

    m_shadowGraphicsPipeline.reset();
    m_shadowRenderPass.reset();
    m_shadowRenderPassDescriptorSetLayout.reset();
    m_lightingRenderPassDescriptorSetLayout.reset();
    m_emptyShadowMapImage.reset();
    m_emptyShadowMap.reset();

    delete m_vsmBlurXComputeDescriptorSet;
    delete m_vsmBlurYComputeDescriptorSet;
//    delete m_vsmBlurIntermediateImageView;
//    delete m_vsmBlurIntermediateImage;
}

bool LightRenderer::init() {

    initEmptyShadowMap();

    m_shadowRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex, true)
            .build();

    m_lightingRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addUniformBuffer(LIGHTING_RENDER_PASS_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addStorageBuffer(LIGHTING_RENDER_PASS_LIGHT_INFO_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addStorageBuffer(LIGHTING_RENDER_PASS_SHADOW_MAP_INFO_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, vk::ShaderStageFlagBits::eFragment, MAX_SHADOW_MAPS)
            .build();


    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_shadowRenderPassResources.set(i, new ShadowRenderPassResources());
        m_lightingRenderPassResources.set(i, new LightingRenderPassResources());
        m_shadowRenderPassResources[i]->cameraInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->lightInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->shadowMapBuffer = nullptr;
        m_lightingRenderPassResources[i]->uniformBuffer = nullptr;

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

        BufferConfiguration bufferConfig;
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.size = sizeof(LightingRenderPassUBO);
        m_lightingRenderPassResources[i]->uniformBuffer = Buffer::create(bufferConfig);

        std::vector<Texture*> emptyShadowMapTextures(MAX_SHADOW_MAPS, m_emptyShadowMap.get());
        DescriptorSetWriter(m_lightingRenderPassResources[i]->descriptorSet)
                .writeBuffer(LIGHTING_RENDER_PASS_UNIFORM_BUFFER_BINDING, m_lightingRenderPassResources[i]->uniformBuffer, 0, sizeof(LightingRenderPassUBO))
                .writeImage(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, emptyShadowMapTextures.data(), vk::ImageLayout::eShaderReadOnlyOptimal, MAX_SHADOW_MAPS)
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
            .addStorageImage(0, vk::ShaderStageFlagBits::eCompute, MAX_SHADOW_MAPS)
            .build();

    m_vsmBlurXComputeDescriptorSet = DescriptorSet::create(m_vsmBlurComputeDescriptorSetLayout, Engine::graphics()->descriptorPool());
    m_vsmBlurYComputeDescriptorSet = DescriptorSet::create(m_vsmBlurComputeDescriptorSetLayout, Engine::graphics()->descriptorPool());

    std::vector<Texture*> emptyShadowMapTextures(MAX_SHADOW_MAPS, m_emptyShadowMap.get());
    DescriptorSetWriter(m_vsmBlurXComputeDescriptorSet)
            .writeImage(0, emptyShadowMapTextures.data(), vk::ImageLayout::eGeneral, MAX_SHADOW_MAPS)
            .write();
    DescriptorSetWriter(m_vsmBlurYComputeDescriptorSet)
            .writeImage(0, emptyShadowMapTextures.data(), vk::ImageLayout::eGeneral, MAX_SHADOW_MAPS)
            .write();

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

    return true;
}

void LightRenderer::preRender(double dt) {
    PROFILE_SCOPE("LightRenderer::preRender")
    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);
    m_numLightEntities = lightEntities.size();

    m_blurElementArrayIndex = 0;

    updateActiveShadowMaps();

    LightingRenderPassUBO uniformData{};
    uniformData.lightCount = m_numLightEntities;
    m_lightingRenderPassResources->uniformBuffer->upload(0, sizeof(LightingRenderPassUBO), &uniformData);
}

void LightRenderer::render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("LightRenderer::render");

    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);

    PROFILE_REGION("Update shadow GPU buffers");

    m_shadowCameraInfoBufferData.clear();
    m_shadowMapBufferData.clear();

    std::vector<RenderCamera> visibleShadowRenderCameras;
    m_visibleShadowMaps.clear();

    for (const auto& id: lightEntities) {
        const LightComponent& lightComponent = lightEntities.get<LightComponent>(id);

        if (!lightComponent.isShadowCaster())
            continue;

        ShadowMap* shadowMap = lightComponent.getShadowMap();
        if (shadowMap == nullptr) {
#if _DEBUG
            printf("Error: shadow-casting light has null shadow map\n");
#endif
            continue;
        }

        const Transform& transform = lightEntities.get<Transform>(id);

        // TODO: only render shadows if they are cast onto anything within renderCamera's view
        // Frustum culling of sphere around point lights
        // Occlusion culling for directional lights???

        RenderCamera shadowRenderCamera;
        if (lightComponent.getType() == LightType_Directional) {
            calculateDirectionalShadowRenderCamera(renderCamera, transform.getForwardAxis(), 0.0F, 10.0F, &shadowRenderCamera);
        } else {
            continue;
        }

        shadowRenderCamera.copyCameraData(&m_shadowCameraInfoBufferData.emplace_back());
        GPUShadowMap& gpuShadowMap = m_shadowMapBufferData.emplace_back();
        gpuShadowMap.viewProjectionMatrix = shadowRenderCamera.getViewProjectionMatrix();
        shadowMap->m_index = m_visibleShadowMaps.size();

        visibleShadowRenderCameras.emplace_back(shadowRenderCamera);
        m_visibleShadowMaps.emplace_back(shadowMap);
    }


    updateLightInfoBuffer(m_visibleShadowMaps.size());
    updateCameraInfoBuffer(m_visibleShadowMaps.size());
    updateShadowMapInfoBuffer(m_visibleShadowMaps.size());

    m_shadowRenderPassResources->cameraInfoBuffer->upload(0, sizeof(GPUCamera) * m_shadowCameraInfoBufferData.size(), m_shadowCameraInfoBufferData.data());
    m_lightingRenderPassResources->shadowMapBuffer->upload(0, sizeof(GPUShadowMap) * m_shadowMapBufferData.size(), m_shadowMapBufferData.data());

    PROFILE_REGION("Render shadows");

    std::vector<const ImageView*> shadowMapImages;
    shadowMapImages.reserve(m_visibleShadowMaps.size());

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        RenderCamera& shadowRenderCamera = visibleShadowRenderCameras[i];
        ShadowMap* shadowMap = m_visibleShadowMaps[i];
        shadowMap->update();
        shadowMap->beginRenderPass(commandBuffer, m_shadowRenderPass.get());

        uint32_t w = shadowMap->getResolution().x;
        uint32_t h = shadowMap->getResolution().y;

        m_shadowGraphicsPipeline->setViewport(commandBuffer, 0, 0.0F, 0.0F, (float) w, (float) h, 0.0F, 1.0F);
        m_shadowGraphicsPipeline->setScissor(commandBuffer, 0, 0, 0, w, h);

        vk::DescriptorSet descriptorSets[2] = {
                m_shadowRenderPassResources->descriptorSet->getDescriptorSet(),
                Engine::sceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
        };

        uint32_t dynamicOffsets[1] = { (uint32_t)(sizeof(GPUCamera) * i) };

        m_shadowGraphicsPipeline->bind(commandBuffer);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shadowGraphicsPipeline->getPipelineLayout(), 0, 2, descriptorSets, 1, dynamicOffsets);

        Engine::sceneRenderer()->render(dt, commandBuffer, &shadowRenderCamera);

        commandBuffer.endRenderPass();

        shadowMapImages.emplace_back(shadowMap->getShadowVarianceImageView());
    }

    vsmBlurActiveShadowMaps(commandBuffer);

    DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
            .writeImage(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, m_vsmShadowMapSampler.get(), shadowMapImages.data(), vk::ImageLayout::eShaderReadOnlyOptimal, shadowMapImages.size())
            .write();
}

const std::shared_ptr<RenderPass>& LightRenderer::getRenderPass() const {
    return m_shadowRenderPass;
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
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
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

void LightRenderer::updateActiveShadowMaps() {
    PROFILE_SCOPE("LightRenderer::updateActiveShadowMaps");

    // Mark all shadow maps inactive. The active ones are re-marked, and the ones that remained inactive and processed and cleaned up later.
    for (auto& entry: m_activeShadowMaps)
        entry.second = false;

    size_t numInactiveShadowMaps = m_inactiveShadowMaps.size();

    m_shadowCameraInfoBufferData.clear();

    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);

    for (const auto& id: lightEntities) {
        LightComponent& lightComponent = lightEntities.get<LightComponent>(id);

        bool hasShadows = lightComponent.isShadowCaster()
                          && lightComponent.getType() == LightType_Directional
                          && lightComponent.getShadowResolution().x > 0
                          && lightComponent.getShadowResolution().y > 0;

        if (!hasShadows) {
            continue; // Skip this light
        }

        if (lightComponent.getShadowMap() == nullptr || lightComponent.getShadowMap()->getResolution() != lightComponent.getShadowResolution()) {
            // We do not have a shadow map or the resolution is wrong.

            ShadowMap* shadowMap = nullptr;

            if (!m_inactiveShadowMaps.empty()) {

                // Look for an inactive shadow map with a matching resolution
                for (auto it1 = m_inactiveShadowMaps.begin(); it1 != m_inactiveShadowMaps.end(); ++it1) {
                    if ((*it1)->getResolution() == lightComponent.getShadowResolution()) {
                        shadowMap = *it1;
                        m_inactiveShadowMaps.erase(it1);
                        break;
                    }
                }

                // No matching resolution was found, so we choose the oldest inactive map (front of queue) to repurpose
                if (shadowMap == nullptr) {
                    shadowMap = m_inactiveShadowMaps.front();
                    m_inactiveShadowMaps.pop_front();
                    shadowMap->setResolution(lightComponent.getShadowResolution());
                }
            }

            if (shadowMap == nullptr) {
                shadowMap = new ShadowMap(lightComponent.getShadowResolution());
            }

            lightComponent.setShadowMap(shadowMap);
        }

        ShadowMap* shadowMap = lightComponent.getShadowMap();

        m_activeShadowMaps[shadowMap] = true;
    }

    if (numInactiveShadowMaps > m_inactiveShadowMaps.size())
        printf("%d shadow maps became active\n", numInactiveShadowMaps - m_inactiveShadowMaps.size());

    numInactiveShadowMaps = m_inactiveShadowMaps.size();

    // All shadow maps that remained inactive are moved from the active pool to the inactive pool.
    for (auto it = m_activeShadowMaps.begin(); it != m_activeShadowMaps.end(); ++it) {
        if (!it->second) {
            m_inactiveShadowMaps.emplace_back(it->first);
            it = m_activeShadowMaps.erase(it);
        }
    }

    if (numInactiveShadowMaps < m_inactiveShadowMaps.size())
        printf("%d shadow maps became inactive\n", m_inactiveShadowMaps.size() - numInactiveShadowMaps);
}

void LightRenderer::updateCameraInfoBuffer(size_t maxShadowLights) {
    PROFILE_SCOPE("LightRenderer::updateCameraInfoBuffer");

    if (maxShadowLights < 1) {
        maxShadowLights = 1;
    }

    vk::DeviceSize newBufferSize = sizeof(GPUCamera) * maxShadowLights;

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
                .writeBuffer(0, m_shadowRenderPassResources->cameraInfoBuffer, 0, sizeof(GPUCamera))
                .write();
    }
}

void LightRenderer::updateLightInfoBuffer(size_t maxLights) {
    PROFILE_SCOPE("LightRenderer::updateLightInfoBuffer");

    if (maxLights < 16) {
        maxLights = 16;
    }

    vk::DeviceSize newBufferSize = sizeof(GPULight) * maxLights;

    if (newBufferSize > 0) {
        if (m_lightingRenderPassResources->lightInfoBuffer == nullptr || newBufferSize > m_lightingRenderPassResources->lightInfoBuffer->getSize()) {
            PROFILE_SCOPE("Allocate updateLightInfoBuffer");

            printf("Allocating LightInfoBuffer - %llu lights\n", maxLights);

            BufferConfiguration bufferConfig;
            bufferConfig.device = Engine::graphics()->getDevice();
            bufferConfig.size = newBufferSize;
            bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

            delete m_lightingRenderPassResources->lightInfoBuffer;
            m_lightingRenderPassResources->lightInfoBuffer = Buffer::create(bufferConfig);

            DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                    .writeBuffer(LIGHTING_RENDER_PASS_LIGHT_INFO_BUFFER_BINDING, m_lightingRenderPassResources->lightInfoBuffer, 0, newBufferSize)
                    .write();

            GPULight defaultLight{};
            defaultLight.type = LightType_Invalid;
            defaultLight.intensity = glm::vec4(0.0F);
            defaultLight.worldPosition = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
            defaultLight.worldDirection = glm::vec4(0.0F, 0.0F, 1.0F, 0.0F);
            defaultLight.shadowMapIndex = 0;
            defaultLight.shadowMapCount = 0;
            m_lightBufferData.clear();
            m_lightBufferData.resize(maxLights, defaultLight);
        }
    }

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("LightRenderer::updateLightInfoBuffer/thread_exec")
        const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);

        size_t itOffset = rangeStart;

        auto it = lightEntities.begin() + itOffset;
        for (size_t index = rangeStart; index != rangeEnd; ++index, ++it) {
            const Transform& transform = lightEntities.get<Transform>(*it);
            const LightComponent& lightComponent = lightEntities.get<LightComponent>(*it);
            GPULight& gpuLight = m_lightBufferData[index];
            gpuLight.type = lightComponent.getType();
            gpuLight.worldPosition = glm::vec4(transform.getTranslation(), 1.0F); // TODO: this should probably be view-space position to avoid loosing precision in extremely large scenes. (planet rendering)
            gpuLight.worldDirection = glm::vec4(transform.getForwardAxis(), 0.0F);
            gpuLight.intensity = glm::vec4(lightComponent.getIntensity(), 0.0F);
            if (lightComponent.getShadowMap() != nullptr && lightComponent.getShadowMap()->getIndex() != ((uint32_t)-1)) {
                gpuLight.shadowMapIndex = lightComponent.getShadowMap()->getIndex();
                gpuLight.shadowMapCount = 1;
            } else {
                gpuLight.shadowMapCount = 0;
            }
        }
    };

    // TODO: for scenes with a huge number of lights, multi-thread this loop. Probably not a common scenario though.
    thread_exec(0, m_numLightEntities);

    // TODO: only re-upload what has changed.
    m_lightingRenderPassResources->lightInfoBuffer->upload(0, m_lightBufferData.size() * sizeof(GPULight), m_lightBufferData.data());
}

void LightRenderer::updateShadowMapInfoBuffer(size_t maxShadowLights) {
    PROFILE_SCOPE("LightRenderer::updateShadowMapInfoBuffer");

    if (maxShadowLights < 1) {
        maxShadowLights = 1;
    }

    vk::DeviceSize newBufferSize = sizeof(GPUShadowMap) * maxShadowLights;

    if (newBufferSize > 0) {
        if (m_lightingRenderPassResources->shadowMapBuffer == nullptr || newBufferSize > m_lightingRenderPassResources->shadowMapBuffer->getSize()) {
            PROFILE_SCOPE("Allocate ShadowMapInfoBuffer");

            printf("Allocating ShadowMapInfoBuffer - %llu lights\n", maxShadowLights);

            BufferConfiguration bufferConfig;
            bufferConfig.device = Engine::graphics()->getDevice();
            bufferConfig.size = newBufferSize;
            bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

            delete m_lightingRenderPassResources->shadowMapBuffer;
            m_lightingRenderPassResources->shadowMapBuffer = Buffer::create(bufferConfig);

            DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                    .writeBuffer(LIGHTING_RENDER_PASS_SHADOW_MAP_INFO_BUFFER_BINDING, m_lightingRenderPassResources->shadowMapBuffer, 0, newBufferSize)
                    .write();
        }
    }
}

void LightRenderer::streamLightData() {
}

void LightRenderer::vsmBlurActiveShadowMaps(const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("LightRenderer::vsmBlurActiveShadowMaps");

    if (m_visibleShadowMaps.empty()) {
        return; // Nothing to do
    }

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    const vk::PipelineLayout& pipelineLayout = m_vsmBlurComputePipeline->getPipelineLayout();
    m_vsmBlurComputePipeline->bind(commandBuffer);

    GaussianBlurPushConstants pushConstantData{};
    pushConstantData.blurRadius = 10.0F;

    std::vector<const ImageView*> shadowMapImageViews;
    shadowMapImageViews.reserve(MAX_SHADOW_MAPS);

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        const ImageView* shadowMapImageView = m_visibleShadowMaps[i]->getShadowVarianceImageView();
        shadowMapImageViews.emplace_back(shadowMapImageView);

        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
                                    ImageTransition::FromAny(),
                                    ImageTransition::ShaderReadWrite(vk::PipelineStageFlagBits::eComputeShader));
    }

    for (size_t i = shadowMapImageViews.size(); i < MAX_SHADOW_MAPS; ++i) {
        shadowMapImageViews.emplace_back(shadowMapImageViews[0]);
    }

    DescriptorSetWriter(m_vsmBlurXComputeDescriptorSet)
            .writeImage(0, m_vsmShadowMapSampler.get(), shadowMapImageViews.data(), vk::ImageLayout::eGeneral, shadowMapImageViews.size())
            .write();


    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurXComputeDescriptorSet->getDescriptorSet(), 0, nullptr);

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        pushConstantData.srcSize = m_visibleShadowMaps[i]->getResolution();
        pushConstantData.dstSize = m_visibleShadowMaps[i]->getResolution();
        uint32_t workgroupCountX = INT_DIV_CEIL(pushConstantData.dstSize.x, 8);
        uint32_t workgroupCountY = INT_DIV_CEIL(pushConstantData.dstSize.y, 8);

        pushConstantData.imageIndex = i;

        pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_X;
        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
        m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);

        const ImageView* shadowMapImageView = m_visibleShadowMaps[i]->getShadowVarianceImageView();
        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
                                    ImageTransition::ShaderReadWrite(vk::PipelineStageFlagBits::eComputeShader),
                                    ImageTransition::ShaderReadWrite(vk::PipelineStageFlagBits::eComputeShader));

        pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_Y;
        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
        m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
    }

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        const ImageView* shadowMapImageView = m_visibleShadowMaps[i]->getShadowVarianceImageView();
        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
                                    ImageTransition::ShaderReadWrite(vk::PipelineStageFlagBits::eComputeShader),
                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader));
    }

}

void LightRenderer::blurImage(const vk::CommandBuffer& commandBuffer, const Sampler* sampler, const ImageView* srcImage, const glm::uvec2& srcSize, const ImageView* dstImage, const glm::uvec2& dstSize, const glm::vec2& blurRadius) {
//    PROFILE_SCOPE("LightRenderer::blurImage")
//
//    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
//
//    const vk::PipelineLayout& pipelineLayout = m_vsmBlurComputePipeline->getPipelineLayout();
//    m_vsmBlurComputePipeline->bind(commandBuffer);
//
//    GaussianBlurPushConstants pushConstantData{};
//    pushConstantData.srcSize = srcSize;
//    pushConstantData.dstSize = dstSize;
//    pushConstantData.srcImageIndex = 0;
//    pushConstantData.dstImageIndex = 0;
//
//
//    if (m_vsmBlurIntermediateImage == nullptr || m_vsmBlurIntermediateImage->getWidth() != dstSize.x || m_vsmBlurIntermediateImage->getHeight() != dstSize.y) {
//        printf("Creating intermediate image for gaussian blur [%d x %d]\n", dstSize.x, dstSize.y);
//        delete m_vsmBlurIntermediateImageView;
//        delete m_vsmBlurIntermediateImage;
//
//        Image2DConfiguration imageConfig;
//        imageConfig.device = Engine::graphics()->getDevice();
//        imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
//        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
//        imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
//        imageConfig.setSize(dstSize);
//        m_vsmBlurIntermediateImage = Image2D::create(imageConfig);
//
//        ImageViewConfiguration imageViewConfig;
//        imageViewConfig.device = Engine::graphics()->getDevice();
//        imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
//        imageViewConfig.setImage(m_vsmBlurIntermediateImage);
//        m_vsmBlurIntermediateImageView = ImageView::create(imageViewConfig);
//
//        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
//                                    ImageTransition::FromAny(),
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
//    } else {
//        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
//                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
//    }
//
//
//    ImageUtil::transitionLayout(commandBuffer, srcImage->getImage(), subresourceRange,
//                                ImageTransition::FromAny(),
//                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));
//
//    size_t arrayCount = MAX_SHADOW_MAPS - m_blurElementArrayIndex;
//    std::vector<const Sampler*> samplers(arrayCount, sampler);
//    std::vector<const ImageView*> imageViews0(arrayCount, srcImage);
//    std::vector<const ImageView*> imageViews1(arrayCount, m_vsmBlurIntermediateImageView);
//    std::vector<const ImageView*> imageViews2(arrayCount, dstImage);
//    std::vector<vk::ImageLayout> imageLayouts0(arrayCount, vk::ImageLayout::eShaderReadOnlyOptimal);
//    std::vector<vk::ImageLayout> imageLayouts1(arrayCount, vk::ImageLayout::eGeneral);
//
//    DescriptorSetWriter(m_vsmBlurXComputeDescriptorSet)
//            .writeImage(0, samplers.data(), imageViews0.data(), imageLayouts0.data(), arrayCount, m_blurElementArrayIndex)
//            .writeImage(1, samplers.data(), imageViews1.data(), imageLayouts1.data(), arrayCount, m_blurElementArrayIndex)
//            .write();
//
//    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurXComputeDescriptorSet->getDescriptorSet(), 0, nullptr);
//
//    pushConstantData.blurRadius = blurRadius.x;
//    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_X;
//    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
//    m_vsmBlurComputePipeline->dispatch(commandBuffer, INT_DIV_CEIL(dstSize.x, 16), INT_DIV_CEIL(dstSize.y, 16), 1);
//
//
//
//
//
//
//    ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImage->getImage(), subresourceRange,
//                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader));
//    ImageUtil::transitionLayout(commandBuffer, dstImage->getImage(), subresourceRange,
//                                ImageTransition::FromAny(),
//                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
//
//    DescriptorSetWriter(m_vsmBlurYComputeDescriptorSet)
//            .writeImage(0, samplers.data(), imageViews1.data(), imageLayouts0.data(), arrayCount, m_blurElementArrayIndex)
//            .writeImage(1, samplers.data(), imageViews2.data(), imageLayouts1.data(), arrayCount, m_blurElementArrayIndex)
//            .write();
////    DescriptorSetWriter(m_vsmBlurYComputeDescriptorSet)
////            .writeImage(0, sampler, m_vsmBlurIntermediateImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0)
////            .writeImage(1, sampler, dstImage, vk::ImageLayout::eGeneral, 0)
////            .write();
//
//    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurYComputeDescriptorSet->getDescriptorSet(), 0, nullptr);
//
//    pushConstantData.blurRadius = blurRadius.y;
//    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_Y;
//    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
//    m_vsmBlurComputePipeline->dispatch(commandBuffer, INT_DIV_CEIL(dstSize.x, 16), INT_DIV_CEIL(dstSize.y, 16), 1);
//
//    ImageUtil::transitionLayout(commandBuffer, dstImage->getImage(), subresourceRange,
//                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader));
//
//    ++m_blurElementArrayIndex;
}


void LightRenderer::calculateDirectionalShadowRenderCamera(const RenderCamera* viewerRenderCamera, const glm::vec3& direction, const double& nearDistance, const double& farDistance, RenderCamera* outShadowRenderCamera) {
    PROFILE_SCOPE("LightRenderer::calculateDirectionalShadowRenderCamera");

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
    shadowCameraTransform.setRotation(direction, glm::vec3(0.0F, 1.0F, 0.0F));
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
