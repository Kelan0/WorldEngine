
#ifndef WORLDENGINE_LIGHTRENDERER_H
#define WORLDENGINE_LIGHTRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/RenderLight.h"

class ShadowMap;
class LightComponent;
class GraphicsPipeline;
class ComputePipeline;
class RenderPass;
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class Texture;
class ImageView;

#define MAX_SHADOW_MAPS 1024
#define MAX_SIMULTANEOUS_VSM_BLUR 8


struct LightingRenderPassUBO {
    uint32_t lightCount;
};


class LightRenderer {
public:
    LightRenderer();

    ~LightRenderer();

    bool init();

    void preRender(const double& dt);

    void render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    const SharedResource<RenderPass>& getRenderPass() const;

    const std::shared_ptr<Texture>& getEmptyShadowMap() const;

    const SharedResource<DescriptorSetLayout>& getLightingRenderPassDescriptorSetLayout() const;

    DescriptorSet* getLightingRenderPassDescriptorSet() const;

    const SharedResource<DescriptorSetLayout>& getVsmBlurComputeDescriptorSetLayout() const;

    const std::shared_ptr<Sampler>& getVsmShadowMapSampler() const;


private:
    void initEmptyShadowMap();

    void updateActiveShadowMaps();

    void markShadowMapInactive(ShadowMap* shadowMap);

    ShadowMap* getShadowMap(const uint32_t& width, const uint32_t& height, const ShadowMap::ShadowType& shadowType, const ShadowMap::RenderType& renderType);

    size_t getNumInactiveShadowMaps() const;

    void updateCameraInfoBuffer(size_t maxShadowLights);

    void updateLightInfoBuffer(size_t maxLights);

    void updateShadowMapInfoBuffer(size_t maxShadowLights);

    void prepareVsmBlurDescriptorSets();

    void prepareVsmBlurIntermediateImage(const vk::CommandBuffer& commandBuffer, const uint32_t& maxWidth, const uint32_t& maxHeight);

    void vsmBlurShadowImage(const vk::CommandBuffer& commandBuffer, const glm::uvec2& resolution, const vk::Image& varianceShadowImage, const vk::Image& intermediateImage, const vk::DescriptorSet& descriptorSetBlurX, const vk::DescriptorSet& descriptorSetBlurY);

    void vsmBlurActiveShadowMaps(const vk::CommandBuffer& commandBuffer);

    void calculateDirectionalShadowCascadeRenderCamera(const RenderCamera* viewerRenderCamera, const Transform& lightTransform, const double& cascadeStartDist, const double& cascadeEndDist, const double& shadowNearPlane, const double shadowFarPlane, RenderCamera* outShadowRenderCamera);

private:
    struct VSMBlurResources {
//        DescriptorSet* descriptorSet;
        std::vector<DescriptorSet*> descriptorSetsBlurX;
        std::vector<DescriptorSet*> descriptorSetsBlurY;
    };

    struct ShadowRenderPassResources {
        DescriptorSet* descriptorSet;
        Buffer* cameraInfoBuffer;
    };

    struct LightingRenderPassResources {
        DescriptorSet* descriptorSet;
        Buffer* lightInfoBuffer;
        Buffer* shadowMapBuffer;
        Buffer* uniformBuffer;
    };

    std::shared_ptr<GraphicsPipeline> m_shadowGraphicsPipeline;
    SharedResource<RenderPass> m_shadowRenderPass;
    SharedResource<DescriptorSetLayout> m_shadowRenderPassDescriptorSetLayout;
    SharedResource<DescriptorSetLayout> m_lightingRenderPassDescriptorSetLayout;

    std::shared_ptr<ComputePipeline> m_vsmBlurComputePipeline;
//    std::shared_ptr<DescriptorSetLayout> m_vsmBlurComputeDescriptorSetLayout;
    SharedResource<DescriptorSetLayout> m_vsmBlurXComputeDescriptorSetLayout;
    uint32_t m_blurElementArrayIndex;

    FrameResource<VSMBlurResources> m_vsmBlurResources;

    FrameResource<ShadowRenderPassResources> m_shadowRenderPassResources;
    FrameResource<LightingRenderPassResources> m_lightingRenderPassResources;

    std::shared_ptr<Image2D> m_emptyShadowMapImage;
    std::shared_ptr<Texture> m_emptyShadowMap;

    std::shared_ptr<Sampler> m_vsmShadowMapSampler;

    Image2D* m_vsmBlurIntermediateImage;
    ImageView* m_vsmBlurIntermediateImageView;

    std::vector<ShadowMap*> m_visibleShadowMaps;
    std::unordered_map<ShadowMap*, bool> m_activeShadowMaps;
    std::unordered_map<ShadowMap::ShadowType, std::vector<ShadowMap*>> m_inactiveShadowMaps;

    std::vector<GPULight> m_lightBufferData;
    std::vector<GPUShadowMap> m_shadowMapBufferData;
    std::vector<GPUCamera> m_shadowCameraInfoBufferData;

    uint32_t m_numLightEntities;
};


#endif //WORLDENGINE_LIGHTRENDERER_H
