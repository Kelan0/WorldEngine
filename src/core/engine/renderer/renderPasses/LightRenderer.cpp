
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
#include "core/util/Profiler.h"
#include "core/util/Util.h"

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


LightRenderer::LightRenderer():
    m_blurElementArrayIndex(0),
    m_vsmBlurIntermediateImageView(nullptr),
    m_vsmBlurIntermediateImage(nullptr),
    m_numLightEntities(0) {
}

LightRenderer::~LightRenderer() {
    delete m_vsmBlurIntermediateImageView;
    delete m_vsmBlurIntermediateImage;

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_shadowRenderPassResources[i]->descriptorSet;
        delete m_shadowRenderPassResources[i]->cameraInfoBuffer;
        delete m_lightingRenderPassResources[i]->descriptorSet;
        delete m_lightingRenderPassResources[i]->lightInfoBuffer;
        delete m_lightingRenderPassResources[i]->shadowMapBuffer;
        delete m_lightingRenderPassResources[i]->uniformBuffer;
//        delete m_vsmBlurResources[i]->descriptorSet;

        for (auto& descriptorSet : m_vsmBlurResources[i]->descriptorSetsBlurX)
            delete descriptorSet;
        for (auto& descriptorSet : m_vsmBlurResources[i]->descriptorSetsBlurY)
            delete descriptorSet;
    }

    for (auto& entry : m_inactiveShadowMaps) {
        for (auto& shadowMap : entry.second)
            delete shadowMap;
    }
    for (auto& entry: m_activeShadowMaps)
        delete entry.first;

    m_shadowGraphicsPipeline.reset();
    m_shadowRenderPass.reset();
    m_shadowRenderPassDescriptorSetLayout.reset();
    m_lightingRenderPassDescriptorSetLayout.reset();
    m_emptyShadowMapImage.reset();
    m_emptyShadowMap.reset();
}

bool LightRenderer::init() {

    initEmptyShadowMap();

    m_shadowRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addUniformBuffer(0, vk::ShaderStageFlagBits::eVertex, true)
            .build("LightRenderer-ShadowRenderPassDescriptorSetLayout");

    m_lightingRenderPassDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addUniformBuffer(LIGHTING_RENDER_PASS_UNIFORM_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addStorageBuffer(LIGHTING_RENDER_PASS_LIGHT_INFO_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addStorageBuffer(LIGHTING_RENDER_PASS_SHADOW_MAP_INFO_BUFFER_BINDING, vk::ShaderStageFlagBits::eFragment)
            .addCombinedImageSampler(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, vk::ShaderStageFlagBits::eFragment, MAX_SHADOW_MAPS)
            .build("LightRenderer-LightingRenderPassDescriptorSetLayout");


    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_shadowRenderPassResources.set(i, new ShadowRenderPassResources());
        m_lightingRenderPassResources.set(i, new LightingRenderPassResources());
        m_shadowRenderPassResources[i]->cameraInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->lightInfoBuffer = nullptr;
        m_lightingRenderPassResources[i]->shadowMapBuffer = nullptr;
        m_lightingRenderPassResources[i]->uniformBuffer = nullptr;

        m_shadowRenderPassResources[i]->descriptorSet = DescriptorSet::create(m_shadowRenderPassDescriptorSetLayout, Engine::graphics()->descriptorPool(), "LightRenderer-ShadowRenderPassDescriptorSet");
        if (m_shadowRenderPassResources[i]->descriptorSet == nullptr) {
            printf("LightRenderer::init - Failed to create camera info descriptor set\n");
            return false;
        }

        m_lightingRenderPassResources[i]->descriptorSet = DescriptorSet::create(m_lightingRenderPassDescriptorSetLayout, Engine::graphics()->descriptorPool(), "LightRenderer-LightingRenderPassDescriptorSet");
        if (m_lightingRenderPassResources[i]->descriptorSet == nullptr) {
            printf("LightRenderer::init - Failed to create camera info descriptor set\n");
            return false;
        }

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.size = sizeof(LightingRenderPassUBO);
        m_lightingRenderPassResources[i]->uniformBuffer = Buffer::create(bufferConfig, "LightRenderer-LightingRenderPass-UniformBuffer");

        std::vector<Texture*> emptyShadowMapTextures(MAX_SHADOW_MAPS, m_emptyShadowMap.get());
        DescriptorSetWriter(m_lightingRenderPassResources[i]->descriptorSet)
                .writeBuffer(LIGHTING_RENDER_PASS_UNIFORM_BUFFER_BINDING, m_lightingRenderPassResources[i]->uniformBuffer, 0, sizeof(LightingRenderPassUBO))
                .writeImage(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, emptyShadowMapTextures.data(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, MAX_SHADOW_MAPS)
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

    SubpassConfiguration subpassConfiguration{};
    subpassConfiguration.addColourAttachment(0, vk::ImageLayout::eColorAttachmentOptimal);
    subpassConfiguration.setDepthStencilAttachment(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.setAttachments(attachments);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.setClearColour(0, glm::vec4(1.0F));
    renderPassConfig.setClearDepth(1, 1.0F);

    m_shadowRenderPass = SharedResource<RenderPass>(RenderPass::create(renderPassConfig, "LightRenderer-ShadowRenderPass"));
    if (!m_shadowRenderPass) {
        printf("LightRenderer::init - Failed to create render pass\n");
        return false;
    }

    GraphicsPipelineConfiguration pipelineConfig{};
    pipelineConfig.device = Engine::graphics()->getDevice();
    pipelineConfig.renderPass = m_shadowRenderPass;
    pipelineConfig.setViewport(512, 512);
    pipelineConfig.vertexShader = "shaders/shadow/shadow.vert";
    pipelineConfig.fragmentShader = "shaders/shadow/shadow.frag";
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

    m_shadowGraphicsPipeline = std::shared_ptr<GraphicsPipeline>(GraphicsPipeline::create(pipelineConfig, "LightRenderer-ShadowGraphicsPipeline"));
    if (!m_shadowGraphicsPipeline) {
        printf("LightRenderer::init - Failed to create graphics pipeline\n");
        return false;
    }

//    m_vsmBlurComputeDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
//            .addStorageImage(0, vk::ShaderStageFlagBits::eCompute, MAX_SIMULTANEOUS_VSM_BLUR)
//            .build("LightRenderer-VsmBlurComputeDescriptorSetLayout");
    m_vsmBlurXComputeDescriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
            .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eCompute, 1)
            .addStorageImage(1, vk::ShaderStageFlagBits::eCompute, 1)
            .build("LightRenderer-VsmBlurXComputeDescriptorSetLayout");

    for (size_t i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_vsmBlurResources.set(i, new VSMBlurResources());
//        m_vsmBlurResources[i]->descriptorSet = DescriptorSet::create(m_vsmBlurComputeDescriptorSetLayout, Engine::graphics()->descriptorPool(), "LightRenderer-VsmBlurComputeDescriptorSet");

//        DescriptorSetWriter(m_vsmBlurResources[i]->descriptorSet)
//            .writeImage(0, m_emptyShadowMap.get(), vk::ImageLayout::eGeneral, 0, MAX_SIMULTANEOUS_VSM_BLUR)
//            .write();
    }

    ComputePipelineConfiguration computePipelineConfig{};
    computePipelineConfig.device = Engine::graphics()->getDevice();
    computePipelineConfig.computeShader = "shaders/compute/compute_gaussianBlur.glsl";
    computePipelineConfig.addDescriptorSetLayout(m_vsmBlurXComputeDescriptorSetLayout.get());
    computePipelineConfig.addPushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants));
    m_vsmBlurComputePipeline = std::shared_ptr<ComputePipeline>(ComputePipeline::create(computePipelineConfig, "LightRenderer-VsmBlurComputePipeline"));

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eLinear;
    samplerConfig.magFilter = vk::Filter::eLinear;
    samplerConfig.wrapU = vk::SamplerAddressMode::eClampToEdge;
    samplerConfig.wrapV = vk::SamplerAddressMode::eClampToEdge;
    m_vsmShadowMapSampler = std::shared_ptr<Sampler>(Sampler::create(samplerConfig, "LightRenderer-VsmShadowMapSampler"));

    return true;
}

void LightRenderer::preRender(const double& dt) {
    PROFILE_SCOPE("LightRenderer::preRender")
    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);
    m_numLightEntities = (uint32_t)lightEntities.size();

    m_blurElementArrayIndex = 0;

    updateActiveShadowMaps();

    LightingRenderPassUBO uniformData{};
    uniformData.lightCount = m_numLightEntities;
    m_lightingRenderPassResources->uniformBuffer->upload(0, sizeof(LightingRenderPassUBO), &uniformData);
}

void LightRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("LightRenderer::render");
    PROFILE_BEGIN_GPU_CMD("LightRenderer::render", commandBuffer);

    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);

    PROFILE_REGION("Update shadow GPU buffers");

    m_shadowCameraInfoBufferData.clear();
    m_shadowMapBufferData.clear();

    std::vector<RenderCamera> visibleShadowRenderCameras;
    m_visibleShadowMaps.clear();

    for (const auto& id : lightEntities) {
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

        shadowMap->update();

        const Transform& transform = lightEntities.get<Transform>(id);

        // TODO: only render shadows if they are cast onto anything within renderCamera's view
        // Frustum culling of sphere around point lights
        // Occlusion culling for directional lights???


        if (shadowMap->getShadowType() == ShadowMap::ShadowType_CascadedShadowMap) {
            CascadedShadowMap* cascadedShadowMap = dynamic_cast<CascadedShadowMap*>(shadowMap);

            shadowMap->m_index = (uint32_t)visibleShadowRenderCameras.size();

            double cascadeStartDistance = 0.0;
            for (size_t i = 0; i < cascadedShadowMap->getNumCascades(); ++i) {
                const double& cascadeEndDistance = cascadedShadowMap->getCascadeSplitDistance(i);
                RenderCamera& shadowRenderCamera = visibleShadowRenderCameras.emplace_back(RenderCamera{});
                const double nearPlane = -64.0F;
                const double farPlane = +64.0F;
                calculateDirectionalShadowCascadeRenderCamera(renderCamera, transform, cascadeStartDistance, cascadeEndDistance, nearPlane, farPlane, &shadowRenderCamera);
                shadowRenderCamera.copyCameraData(&m_shadowCameraInfoBufferData.emplace_back());
                GPUShadowMap& gpuShadowMap = m_shadowMapBufferData.emplace_back();
                gpuShadowMap.viewProjectionMatrix = shadowRenderCamera.getViewProjectionMatrix();
                gpuShadowMap.cascadeStartZ = (float)cascadeStartDistance;
                gpuShadowMap.cascadeEndZ = (float)cascadeEndDistance;

                cascadeStartDistance = cascadeEndDistance;
            }
        } else {
            continue;
        }

//        if (lightComponent.getType() == LightType_Directional) {
//            calculateDirectionalShadowCascadeRenderCamera(renderCamera, transform, 0.0F, 5.0F, -64.0, +64.0, &shadowRenderCamera);
//        } else {
//            continue;
//        }
//
//        shadowRenderCamera.copyCameraData(&m_shadowCameraInfoBufferData.emplace_back());
//        GPUShadowMap& gpuShadowMap = m_shadowMapBufferData.emplace_back();
//        gpuShadowMap.viewProjectionMatrix = shadowRenderCamera.getViewProjectionMatrix();
//        shadowMap->m_index = (uint32_t)m_visibleShadowMaps.size();
//        visibleShadowRenderCameras.emplace_back(shadowRenderCamera);

        m_visibleShadowMaps.emplace_back(shadowMap);
    }

    // We must call these methods in order to initialize the descriptor set used by the lighting render pass.
    updateLightInfoBuffer(m_visibleShadowMaps.size());
    updateShadowMapInfoBuffer(m_shadowMapBufferData.size());

    if (m_visibleShadowMaps.empty())
        return; // Nothing more to do.

    updateCameraInfoBuffer(m_shadowCameraInfoBufferData.size());

    m_shadowRenderPassResources->cameraInfoBuffer->upload(0, sizeof(GPUCamera) * m_shadowCameraInfoBufferData.size(), m_shadowCameraInfoBufferData.data());
    m_lightingRenderPassResources->shadowMapBuffer->upload(0, sizeof(GPUShadowMap) * m_shadowMapBufferData.size(), m_shadowMapBufferData.data());

    PROFILE_REGION("Render shadows");

    std::vector<const ImageView*> shadowMapImages;
    shadowMapImages.reserve(visibleShadowRenderCameras.size());

    uint32_t cameraInfoBufferIndex = 0;

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        PROFILE_BEGIN_GPU_CMD("LightRenderer::render/ShadowMapRenderPass", commandBuffer);
        ShadowMap* shadowMap = m_visibleShadowMaps[i];

        m_shadowGraphicsPipeline->setViewport(commandBuffer, 0, shadowMap->getResolution());
        m_shadowGraphicsPipeline->setScissor(commandBuffer, 0, glm::ivec2(0, 0), shadowMap->getResolution());

        std::array<vk::DescriptorSet, 2> descriptorSets = {
                m_shadowRenderPassResources->descriptorSet->getDescriptorSet(),
                Engine::sceneRenderer()->getObjectDescriptorSet()->getDescriptorSet(),
        };

        m_shadowGraphicsPipeline->bind(commandBuffer);

        if (shadowMap->getShadowType() == ShadowMap::ShadowType_CascadedShadowMap) {
            CascadedShadowMap* cascadedShadowMap = dynamic_cast<CascadedShadowMap*>(shadowMap);
            for (size_t j = 0; j < cascadedShadowMap->getNumCascades(); ++j) {
                PROFILE_BEGIN_GPU_CMD("LightRenderer::render/ShadowMapCascadeRenderPass", commandBuffer);

                RenderCamera& shadowRenderCamera = visibleShadowRenderCameras[shadowMap->m_index + j];

                std::array<uint32_t, 1> dynamicOffsets = { (uint32_t)(sizeof(GPUCamera) * cameraInfoBufferIndex++) };
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shadowGraphicsPipeline->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

                m_shadowRenderPass->begin(commandBuffer, cascadedShadowMap->getCascadeFramebuffer(j), vk::SubpassContents::eInline);
                Engine::sceneRenderer()->render(dt, commandBuffer, &shadowRenderCamera);
                commandBuffer.endRenderPass();

                shadowMapImages.emplace_back(cascadedShadowMap->getCascadeShadowVarianceImageView(j));

                PROFILE_END_GPU_CMD(commandBuffer);
            }
        }

//        RenderCamera& shadowRenderCamera = visibleShadowRenderCameras[shadowMap->m_index];
//        shadowMap->beginRenderPass(commandBuffer, m_shadowRenderPass.get());
//        Engine::sceneRenderer()->render(dt, commandBuffer, &shadowRenderCamera);
//        commandBuffer.endRenderPass();
//        shadowMapImages.emplace_back(shadowMap->getShadowVarianceImageView());

        PROFILE_END_GPU_CMD(commandBuffer);
    }

    vsmBlurActiveShadowMaps(commandBuffer);

    DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
            .writeImage(LIGHTING_RENDER_PASS_SHADOW_DEPTH_TEXTURES_BINDING, m_vsmShadowMapSampler.get(), shadowMapImages.data(), vk::ImageLayout::eShaderReadOnlyOptimal, 0, (uint32_t)shadowMapImages.size())
            .write();

    PROFILE_END_GPU_CMD(commandBuffer);
}

const SharedResource<RenderPass>& LightRenderer::getRenderPass() const {
    return m_shadowRenderPass;
}

const std::shared_ptr<Texture>& LightRenderer::getEmptyShadowMap() const {
    return m_emptyShadowMap;
}

const SharedResource<DescriptorSetLayout>& LightRenderer::getLightingRenderPassDescriptorSetLayout() const {
    return m_lightingRenderPassDescriptorSetLayout;
}

DescriptorSet* LightRenderer::getLightingRenderPassDescriptorSet() const {
    return m_lightingRenderPassResources->descriptorSet;
}

const SharedResource<DescriptorSetLayout>& LightRenderer::getVsmBlurComputeDescriptorSetLayout() const {
    return m_vsmBlurXComputeDescriptorSetLayout;
}

const std::shared_ptr<Sampler>& LightRenderer::getVsmShadowMapSampler() const {
    return m_vsmShadowMapSampler;
}

void LightRenderer::initEmptyShadowMap() {
    union {
        float depth;
        uint8_t pixelData[4];
    };
    depth = 1.0F;
    ImageData imageData(pixelData, 1, 1, ImagePixelLayout::R, ImagePixelFormat::Float32);

    Image2DConfiguration imageConfig{};
    imageConfig.device = Engine::graphics()->getDevice();
    imageConfig.format = vk::Format::eR32Sfloat;
    imageConfig.setSize(1, 1);
    imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    imageConfig.imageData = &imageData;
    m_emptyShadowMapImage = std::shared_ptr<Image2D>(Image2D::create(imageConfig, "LightRenderer-EmptyShadowMapImage"));

    ImageViewConfiguration emptyShadowMapImageViewConfig{};
    emptyShadowMapImageViewConfig.device = Engine::graphics()->getDevice();
    emptyShadowMapImageViewConfig.format = vk::Format::eR32Sfloat;
    emptyShadowMapImageViewConfig.aspectMask = vk::ImageAspectFlagBits::eColor;
    emptyShadowMapImageViewConfig.setImage(m_emptyShadowMapImage.get());

    SamplerConfiguration samplerConfig{};
    samplerConfig.device = Engine::graphics()->getDevice();
    samplerConfig.minFilter = vk::Filter::eNearest;
    samplerConfig.magFilter = vk::Filter::eNearest;

    m_emptyShadowMap = std::shared_ptr<Texture>(Texture::create(emptyShadowMapImageViewConfig, samplerConfig, "LightRenderer-EmptyShadowMapTexture"));
}

void LightRenderer::updateActiveShadowMaps() {
    PROFILE_SCOPE("LightRenderer::updateActiveShadowMaps");

    // Mark all shadow maps inactive. The active ones are re-marked, and the ones that remained inactive and processed and cleaned up later.
    for (auto& entry: m_activeShadowMaps)
        entry.second = false;

    m_shadowCameraInfoBufferData.clear();

    const auto& lightEntities = Engine::scene()->registry()->group<LightComponent>(entt::get<Transform>);

    for (const auto& id: lightEntities) {
        LightComponent &lightComponent = lightEntities.get<LightComponent>(id);

        bool hasShadows = lightComponent.isShadowCaster()
                          && lightComponent.getType() == LightType_Directional
                          && lightComponent.getShadowResolution().x > 0
                          && lightComponent.getShadowResolution().y > 0;

        if (!hasShadows) {
            continue; // Skip this light
        }

        if (lightComponent.getShadowMap() == nullptr || lightComponent.getShadowMap()->getResolution() != lightComponent.getShadowResolution()) {
            // We do not have a shadow map or the resolution is wrong.

            if (lightComponent.getShadowMap() != nullptr) {
                markShadowMapInactive(lightComponent.getShadowMap());
            }

            ShadowMap* shadowMap = nullptr;
            const std::vector<double>& cascadeDistances = lightComponent.getShadowCascadeDistances();
            if (!cascadeDistances.empty()) {
                shadowMap = getShadowMap(lightComponent.getShadowResolution().x, lightComponent.getShadowResolution().y,ShadowMap::ShadowType_CascadedShadowMap,ShadowMap::RenderType_VarianceShadowMap);
                CascadedShadowMap* cascadedShadowMap = dynamic_cast<CascadedShadowMap*>(shadowMap);
                cascadedShadowMap->setNumCascades(cascadeDistances.size());
                for (size_t i = 0; i < cascadeDistances.size(); ++i) {
                    cascadedShadowMap->setCascadeSplitDistance(i, cascadeDistances[i]);
                }
            }
            lightComponent.setShadowMap(shadowMap);
        }

        m_activeShadowMaps[lightComponent.getShadowMap()] = true;
    }

    // All shadow maps that remained inactive are moved from the active pool to the inactive pool.
    for (auto it = m_activeShadowMaps.begin(); it != m_activeShadowMaps.end(); ++it) {
        if (!it->second) {
            markShadowMapInactive(it->first);
            it = m_activeShadowMaps.erase(it);
        }
    }
}


void LightRenderer::markShadowMapInactive(ShadowMap* shadowMap) {
    std::vector<ShadowMap*>& inactiveShadowMaps = m_inactiveShadowMaps[shadowMap->getShadowType()];
    auto it = std::upper_bound(inactiveShadowMaps.begin(), inactiveShadowMaps.end(), shadowMap, [](const ShadowMap* lhs, const ShadowMap* rhs) {
        return (lhs->getResolution().x < rhs->getResolution().x) || (lhs->getResolution().x == rhs->getResolution().x && lhs->getResolution().y < rhs->getResolution().y);
    });

    if (it != inactiveShadowMaps.end() && (*it) == shadowMap) {
        return; // Already inactive.
    }

    inactiveShadowMaps.insert(it, shadowMap);
}

ShadowMap* LightRenderer::getShadowMap(const uint32_t& width, const uint32_t& height, const ShadowMap::ShadowType& shadowType, const ShadowMap::RenderType& renderType) {
    std::vector<ShadowMap*>& inactiveShadowMaps = m_inactiveShadowMaps[shadowType];
    auto it = std::upper_bound(inactiveShadowMaps.begin(), inactiveShadowMaps.end(), glm::uvec2(width, height), [](const glm::uvec2& lhs, const ShadowMap* rhs) {
        return (lhs.x < rhs->getResolution().x) || (lhs.x == rhs->getResolution().x && lhs.y < rhs->getResolution().y);
    });

    ShadowMap* shadowMap = nullptr;

    if (it != inactiveShadowMaps.end()) {
        shadowMap = *it;
        inactiveShadowMaps.erase(it);
        assert(shadowMap->getShadowType() == shadowType);
    } else {
        printf("Allocating new shadow map: [%llu x %llu]\n", (uint64_t)width, (uint64_t)height);

        switch (shadowType) {
            case ShadowMap::ShadowType_CascadedShadowMap:
                shadowMap = new CascadedShadowMap(renderType);
                break;
            default:
                printf("LightRenderer::getShadowMap: Invalid ShadowType\n");
                assert(false);
                break;
        }
    }

    shadowMap->setResolution(width, height);

    return shadowMap;
}

size_t LightRenderer::getNumInactiveShadowMaps() const {
    size_t count = 0;
    for (const auto& entry : m_inactiveShadowMaps) {
        count += entry.second.size();
    }
    return count;
}

void LightRenderer::updateCameraInfoBuffer(size_t maxShadowLights) {
    PROFILE_SCOPE("LightRenderer::updateCameraInfoBuffer");

    if (maxShadowLights < 1) {
        maxShadowLights = 1;
    }

    vk::DeviceSize newBufferSize = sizeof(GPUCamera) * maxShadowLights;

    if (m_shadowRenderPassResources->cameraInfoBuffer == nullptr || newBufferSize > m_shadowRenderPassResources->cameraInfoBuffer->getSize()) {
        PROFILE_SCOPE("Allocate CameraInfoBuffer");

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = newBufferSize;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_shadowRenderPassResources->cameraInfoBuffer;
        m_shadowRenderPassResources->cameraInfoBuffer = Buffer::create(bufferConfig, "LightRenderer-ShadowRenderPass-CameraInfoUniformBuffer");

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

            BufferConfiguration bufferConfig{};
            bufferConfig.device = Engine::graphics()->getDevice();
            bufferConfig.size = newBufferSize;
            bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

            delete m_lightingRenderPassResources->lightInfoBuffer;
            m_lightingRenderPassResources->lightInfoBuffer = Buffer::create(bufferConfig, "LightRenderer-LightingRenderPass-LightingInfoStorageBuffer");

            DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                    .writeBuffer(LIGHTING_RENDER_PASS_LIGHT_INFO_BUFFER_BINDING, m_lightingRenderPassResources->lightInfoBuffer, 0, newBufferSize)
                    .write();

            GPULight defaultLight{};
            defaultLight.type = LightType_Invalid;
            defaultLight.intensity = glm::vec4(0.0F);
            defaultLight.worldPosition = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
            defaultLight.worldDirection = glm::vec4(0.0F, 0.0F, 1.0F, 0.0F);
            defaultLight.cosAngularSize = 1.0F; // 0 degrees
            defaultLight.shadowMapIndex = 0;
            defaultLight.shadowMapCount = 0;
            defaultLight.flags = 0;
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
            gpuLight.cosAngularSize = glm::cos(lightComponent.getAngularSize());
            gpuLight.shadowMapCount = 0;

            ShadowMap* shadowMap = lightComponent.getShadowMap();
            if (shadowMap != nullptr && shadowMap->m_index != ((uint32_t)-1)) {
                gpuLight.flags_csmMapBasedSelection = lightComponent.isCsmMapBasedSelection();
                gpuLight.shadowMapIndex = shadowMap->m_index;
                if (shadowMap->getShadowType() == ShadowMap::ShadowType_CascadedShadowMap) {
                    gpuLight.shadowMapCount = (uint32_t)dynamic_cast<CascadedShadowMap*>(shadowMap)->getNumCascades();
                } else {
//                    gpuLight.shadowMapCount = 1;
                }
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

            BufferConfiguration bufferConfig{};
            bufferConfig.device = Engine::graphics()->getDevice();
            bufferConfig.size = newBufferSize;
            bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
            bufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

            delete m_lightingRenderPassResources->shadowMapBuffer;
            m_lightingRenderPassResources->shadowMapBuffer = Buffer::create(bufferConfig, "LightRenderer-LightingRenderPass-ShadowMapStorageBuffer");

            DescriptorSetWriter(m_lightingRenderPassResources->descriptorSet)
                    .writeBuffer(LIGHTING_RENDER_PASS_SHADOW_MAP_INFO_BUFFER_BINDING, m_lightingRenderPassResources->shadowMapBuffer, 0, newBufferSize)
                    .write();
        }
    }
}

void LightRenderer::prepareVsmBlurDescriptorSets() {
//    PROFILE_SCOPE("LightRenderer::prepareVsmBlurDescriptorSets");
//
//    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
//        if (m_visibleShadowMaps[i]->getRenderType() != ShadowMap::RenderType_VarianceShadowMap)
//            continue;
//
//        const ImageView* shadowMapImageView = m_visibleShadowMaps[i]->getShadowVarianceImageView();
//
//        if (i >= m_vsmBlurResources->descriptorSetsBlurX.size()) {
//            DescriptorSet* descriptorSetBlurX = DescriptorSet::create(m_vsmBlurXComputeDescriptorSetLayout,Engine::graphics()->descriptorPool(),"LightRenderer-VsmBlurXComputeDescriptorSet");
//            m_vsmBlurResources->descriptorSetsBlurX.emplace_back(descriptorSetBlurX);
//
//            DescriptorSetWriter(m_vsmBlurResources->descriptorSetsBlurX[i])
//                    .writeImage(0, m_vsmShadowMapSampler.get(), shadowMapImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
//                    .writeImage(1, m_vsmShadowMapSampler.get(), m_vsmBlurIntermediateImageView, vk::ImageLayout::eGeneral, 0, 1)
//                    .write();
//        }
//
//        if (i >= m_vsmBlurResources->descriptorSetsBlurY.size()) {
//            DescriptorSet* descriptorSetBlurY = DescriptorSet::create(m_vsmBlurXComputeDescriptorSetLayout,Engine::graphics()->descriptorPool(),"LightRenderer-VsmBlurYComputeDescriptorSet");
//            m_vsmBlurResources->descriptorSetsBlurY.emplace_back(descriptorSetBlurY);
//
//            DescriptorSetWriter(m_vsmBlurResources->descriptorSetsBlurY[i])
//                    .writeImage(0, m_vsmShadowMapSampler.get(), m_vsmBlurIntermediateImageView, vk::ImageLayout::eShaderReadOnlyOptimal, 0, 1)
//                    .writeImage(1, m_vsmShadowMapSampler.get(), shadowMapImageView, vk::ImageLayout::eGeneral, 0, 1)
//                    .write();
//        }
//    }
}

void LightRenderer::prepareVsmBlurIntermediateImage(const vk::CommandBuffer& commandBuffer, const uint32_t& maxWidth, const uint32_t& maxHeight) {
//    PROFILE_SCOPE("LightRenderer::prepareVsmBlurIntermediateImage");
//    PROFILE_BEGIN_GPU_CMD("LightRenderer::prepareVsmBlurIntermediateImage", commandBuffer);
//
//    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
//
//    if (m_vsmBlurIntermediateImage == nullptr || m_vsmBlurIntermediateImage->getWidth() < maxWidth || m_vsmBlurIntermediateImage->getHeight() < maxHeight) {
//        const uint32_t& w = (uint32_t)Util::nextPowerOf2(maxWidth);
//        const uint32_t& h = (uint32_t)Util::nextPowerOf2(maxHeight);
//        printf("Creating intermediate image for gaussian blur [%d x %d]\n", w, h);
//        delete m_vsmBlurIntermediateImageView;
//        delete m_vsmBlurIntermediateImage;
//
//        Image2DConfiguration imageConfig{};
//        imageConfig.device = Engine::graphics()->getDevice();
//        imageConfig.format = vk::Format::eR32G32B32A32Sfloat;
//        imageConfig.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
//        imageConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
//        imageConfig.setSize(w, h);
//        m_vsmBlurIntermediateImage = Image2D::create(imageConfig, "LightRenderer-VsmBlurIntermediateImage");
//
//        ImageViewConfiguration imageViewConfig{};
//        imageViewConfig.device = Engine::graphics()->getDevice();
//        imageViewConfig.format = vk::Format::eR32G32B32A32Sfloat;
//        imageViewConfig.setImage(m_vsmBlurIntermediateImage);
//        m_vsmBlurIntermediateImageView = ImageView::create(imageViewConfig, "LightRenderer-VsmBlurIntermediateImageView");
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
//    PROFILE_END_GPU_CMD(commandBuffer);
}

void LightRenderer::vsmBlurShadowImage(const vk::CommandBuffer& commandBuffer, const glm::uvec2& resolution, const vk::Image& varianceShadowImage, const vk::Image& intermediateImage, const vk::DescriptorSet& descriptorSetBlurX, const vk::DescriptorSet& descriptorSetBlurY) {
    PROFILE_SCOPE("LightRenderer::vsmBlurShadowImage");
    PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurShadowImage", commandBuffer);

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    const vk::PipelineLayout& pipelineLayout = m_vsmBlurComputePipeline->getPipelineLayout();

    GaussianBlurPushConstants pushConstantData{};
    pushConstantData.blurRadius = 10.0F;

    pushConstantData.imageIndex = 0;
    pushConstantData.srcSize = resolution;
    pushConstantData.dstSize = resolution;

    ImageUtil::transitionLayout(commandBuffer, varianceShadowImage, subresourceRange,
                                ImageTransition::FromAny(),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));
    ImageUtil::transitionLayout(commandBuffer, intermediateImage, subresourceRange,
                                ImageTransition::FromAny(),
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));

    constexpr uint32_t workgroupSize = 4;
    uint32_t workgroupCountX = INT_DIV_CEIL(resolution.x, workgroupSize);
    uint32_t workgroupCountY = INT_DIV_CEIL(resolution.y, workgroupSize);

    PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurShadowImage/ComputeBlur_X", commandBuffer);
    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_X;
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &descriptorSetBlurX, 0, nullptr);
    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
    m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
    PROFILE_END_GPU_CMD(commandBuffer);

    ImageUtil::transitionLayout(commandBuffer, varianceShadowImage, subresourceRange,
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader),
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
    ImageUtil::transitionLayout(commandBuffer, intermediateImage, subresourceRange,
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));

    PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurShadowImage/ComputeBlur_Y", commandBuffer);
    pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_Y;
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &descriptorSetBlurY, 0, nullptr);
    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
    m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
    PROFILE_END_GPU_CMD(commandBuffer);

    ImageUtil::transitionLayout(commandBuffer, varianceShadowImage, subresourceRange,
                                ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
                                ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader));

    PROFILE_END_GPU_CMD(commandBuffer);
}

void LightRenderer::vsmBlurActiveShadowMaps(const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("LightRenderer::vsmBlurActiveShadowMaps");
    PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurActiveShadowMaps", commandBuffer);

    if (m_visibleShadowMaps.empty()) {
        return; // Nothing to do
    }

    m_vsmBlurComputePipeline->bind(commandBuffer);

    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
        ShadowMap* shadowMap = m_visibleShadowMaps[i];

        if (shadowMap->getShadowType() == ShadowMap::ShadowType_CascadedShadowMap) {
            CascadedShadowMap* cascadedShadowMap = dynamic_cast<CascadedShadowMap*>(shadowMap);
            for (size_t j = 0; j < cascadedShadowMap->getNumCascades(); ++j) {
                const ImageView* shadowVarianceImageView = cascadedShadowMap->getCascadeShadowVarianceImageView(j);
                const ImageView* vsmBlurIntermediateImageView = cascadedShadowMap->getCascadeVsmBlurIntermediateImageView(j);
                const DescriptorSet* vsmBlurXDescriptorSet = cascadedShadowMap->getCascadeVsmBlurXDescriptorSet(j);
                const DescriptorSet* vsmBlurYDescriptorSet = cascadedShadowMap->getCascadeVsmBlurYDescriptorSet(j);
                vsmBlurShadowImage(commandBuffer, cascadedShadowMap->getResolution(), shadowVarianceImageView->getImage(), vsmBlurIntermediateImageView->getImage(), vsmBlurXDescriptorSet->getDescriptorSet(), vsmBlurYDescriptorSet->getDescriptorSet());
            }
        }
    }



//    uint32_t maxWidth = 0;
//    uint32_t maxHeight = 0;
//    for (auto& shadowMap : m_visibleShadowMaps) {
//        maxWidth = glm::max(maxWidth, shadowMap->getResolution().x);
//        maxHeight = glm::max(maxHeight, shadowMap->getResolution().y);
//    }
//
//    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
//
//    prepareVsmBlurIntermediateImage(commandBuffer, maxWidth, maxHeight);
//
//    prepareVsmBlurDescriptorSets();
//
//    uint32_t workgroupCountX;
//    uint32_t workgroupCountY;
//
//    const vk::PipelineLayout& pipelineLayout = m_vsmBlurComputePipeline->getPipelineLayout();
//
//    GaussianBlurPushConstants pushConstantData{};
//    pushConstantData.blurRadius = 10.0F;
//
//    constexpr uint32_t workgroupSize = 4;
//    for (size_t i = 0; i < m_visibleShadowMaps.size(); ++i) {
//        const ShadowMap* shadowMap = m_visibleShadowMaps[i];
//        const ImageView* shadowMapImageView = shadowMap->getShadowVarianceImageView();
//
//        pushConstantData.imageIndex = 0;
//        pushConstantData.srcSize = m_visibleShadowMaps[i]->getResolution();
//        pushConstantData.dstSize = m_visibleShadowMaps[i]->getResolution();
//
//        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
//                                    ImageTransition::FromAny(),
//                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));
//        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImageView->getImage(), subresourceRange,
//                                    ImageTransition::FromAny(),
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
//
//        PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurActiveShadowMaps/ComputeBlur_X", commandBuffer);
//        pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_X;
//        workgroupCountX = INT_DIV_CEIL(pushConstantData.dstSize.x, workgroupSize);
//        workgroupCountY = INT_DIV_CEIL(pushConstantData.dstSize.y, workgroupSize);
//        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurResources->descriptorSetsBlurX[i]->getDescriptorSet(), 0, nullptr);
//        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
//        m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
//        PROFILE_END_GPU_CMD(commandBuffer);
//
//        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
//                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader));
//        ImageUtil::transitionLayout(commandBuffer, m_vsmBlurIntermediateImageView->getImage(), subresourceRange,
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader));
//
//        PROFILE_BEGIN_GPU_CMD("LightRenderer::vsmBlurActiveShadowMaps/ComputeBlur_Y", commandBuffer);
//        pushConstantData.blurDirection = GAUSSIAN_BLUE_DIRECTION_Y;
//        workgroupCountX = INT_DIV_CEIL(pushConstantData.dstSize.x, workgroupSize);
//        workgroupCountY = INT_DIV_CEIL(pushConstantData.dstSize.y, workgroupSize);
//        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &m_vsmBlurResources->descriptorSetsBlurY[i]->getDescriptorSet(), 0, nullptr);
//        commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(GaussianBlurPushConstants), &pushConstantData);
//        m_vsmBlurComputePipeline->dispatch(commandBuffer, workgroupCountX, workgroupCountY, 1);
//        PROFILE_END_GPU_CMD(commandBuffer);
//
//        ImageUtil::transitionLayout(commandBuffer, shadowMapImageView->getImage(), subresourceRange,
//                                    ImageTransition::ShaderWriteOnly(vk::PipelineStageFlagBits::eComputeShader),
//                                    ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eFragmentShader));
//    }

    PROFILE_END_GPU_CMD(commandBuffer);
}

void LightRenderer::calculateDirectionalShadowCascadeRenderCamera(const RenderCamera* viewerRenderCamera, const Transform& lightTransform, const double& cascadeStartDist, const double& cascadeEndDist, const double& shadowNearPlane, const double shadowFarPlane, RenderCamera* outShadowRenderCamera) {
    PROFILE_SCOPE("LightRenderer::calculateDirectionalShadowCascadeRenderCamera");

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
    viewerFrustumCorners[Frustum::Corner_Left_Top_Far] = viewerFrustumCorners[Frustum::Corner_Left_Top_Near] + dTL * cascadeEndDist * scaleTL;
    viewerFrustumCorners[Frustum::Corner_Right_Top_Far] = viewerFrustumCorners[Frustum::Corner_Right_Top_Near] + dTR * cascadeEndDist * scaleTR;
    viewerFrustumCorners[Frustum::Corner_Right_Bottom_Far] = viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] + dBR * cascadeEndDist * scaleBR;
    viewerFrustumCorners[Frustum::Corner_Left_Bottom_Far] = viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] + dBL * cascadeEndDist * scaleBL;
    viewerFrustumCorners[Frustum::Corner_Left_Top_Near] = viewerFrustumCorners[Frustum::Corner_Left_Top_Near] + dTL * cascadeStartDist * scaleTL;
    viewerFrustumCorners[Frustum::Corner_Right_Top_Near] = viewerFrustumCorners[Frustum::Corner_Right_Top_Near] + dTR * cascadeStartDist * scaleTR;
    viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] = viewerFrustumCorners[Frustum::Corner_Right_Bottom_Near] + dBR * cascadeStartDist * scaleBR;
    viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] = viewerFrustumCorners[Frustum::Corner_Left_Bottom_Near] + dBL * cascadeStartDist * scaleBL;

    // Calculate center of this subsection of the viewer frustum.
    glm::dvec3 worldSpaceFrustumCenter(0.0);
    for (size_t i = 0; i < Frustum::NumCorners; ++i)
        worldSpaceFrustumCenter += viewerFrustumCorners[i];
    worldSpaceFrustumCenter /= Frustum::NumCorners;

    Transform shadowCameraTransform;
    shadowCameraTransform.setTranslation(worldSpaceFrustumCenter);
//    shadowCameraTransform.setRotation(lightTransform.getForwardAxis(), glm::vec3(0.0F, 1.0F, 0.0F));
    shadowCameraTransform.setRotation(lightTransform.getRotationMatrix());
    glm::dmat4 shadowViewMatrix = glm::inverse(shadowCameraTransform.getMatrix());

    glm::dvec2 shadowViewMin(std::numeric_limits<double>::max());
    glm::dvec2 shadowViewMax(std::numeric_limits<double>::min());

    constexpr bool useBoundingSphere = true;
    glm::dvec3 lightSpaceFrustumCenter;

    if (useBoundingSphere) {
        lightSpaceFrustumCenter = glm::dvec3(shadowViewMatrix * glm::dvec4(worldSpaceFrustumCenter, 1.0F));
    }

    double boundingSphereRadius = 0.0;

    if (useBoundingSphere) {
        glm::dvec3 dir;
        for (size_t i = 0; i < Frustum::NumCorners; ++i) {
            temp = shadowViewMatrix * glm::dvec4(viewerFrustumCorners[i], 1.0F); // Light-space frustum corner
            dir = glm::dvec3(temp) - lightSpaceFrustumCenter;
            boundingSphereRadius = glm::max(boundingSphereRadius, glm::dot(dir, dir)); // Use distance^2 until we found the largest, then sqrt it
        }
        boundingSphereRadius = glm::sqrt(boundingSphereRadius);
    } else {
        for (size_t i = 0; i < Frustum::NumCorners; ++i) {
            temp = shadowViewMatrix * glm::dvec4(viewerFrustumCorners[i], 1.0F); // Light-space frustum corner
            shadowViewMin.x = glm::min(shadowViewMin.x, temp.x);
            shadowViewMin.y = glm::min(shadowViewMin.y, temp.y);
            shadowViewMax.x = glm::max(shadowViewMax.x, temp.x);
            shadowViewMax.y = glm::max(shadowViewMax.y, temp.y);
        }
    }

    if (useBoundingSphere) {
        shadowViewMin = glm::dvec2(lightSpaceFrustumCenter.x - boundingSphereRadius, lightSpaceFrustumCenter.y - boundingSphereRadius);
        shadowViewMax = glm::dvec2(lightSpaceFrustumCenter.x + boundingSphereRadius, lightSpaceFrustumCenter.y + boundingSphereRadius);
    }

    Camera shadowCameraProjection(shadowViewMin.x, shadowViewMax.x, shadowViewMin.y, shadowViewMax.y, shadowNearPlane, shadowFarPlane, true);

    outShadowRenderCamera->setProjection(shadowCameraProjection);
    outShadowRenderCamera->setTransform(shadowCameraTransform);
    outShadowRenderCamera->update();
}
