
#ifndef WORLDENGINE_LIGHTRENDERER_H
#define WORLDENGINE_LIGHTRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/ShadowMap.h"
#include "core/engine/renderer/RenderLight.h"

class ShadowMap;
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

    void preRender(double dt);

    void render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    [[nodiscard]] const std::shared_ptr<RenderPass>& getRenderPass() const;

    [[nodiscard]] const std::shared_ptr<Texture>& getEmptyShadowMap() const;

    [[nodiscard]] const std::shared_ptr<DescriptorSetLayout>& getLightingRenderPassDescriptorSetLayout() const;

    [[nodiscard]] DescriptorSet* getLightingRenderPassDescriptorSet() const;

private:
    void initEmptyShadowMap();

    void updateActiveShadowMaps();

    void updateCameraInfoBuffer(size_t maxShadowLights);

    void updateLightInfoBuffer(size_t maxLights);

    void updateShadowMapInfoBuffer(size_t maxShadowLights);

    void streamLightData();

    void blurImage(const vk::CommandBuffer& commandBuffer, const Sampler* sampler, const ImageView* srcImage, const glm::uvec2& srcSize, const ImageView* dstImage, const glm::uvec2& dstSize, const glm::vec2& blurRadius);

    void prepareVsmBlurDescriptorSets();

    void prepareVsmBlurIntermediateImage(const vk::CommandBuffer& commandBuffer, const uint32_t& maxWidth, const uint32_t& maxHeight);

    void vsmBlurActiveShadowMaps(const vk::CommandBuffer& commandBuffer);

    void calculateDirectionalShadowRenderCamera(const RenderCamera* viewerRenderCamera, const Transform& lightTransform, const double& cascadeStartDist, const double& cascadeEndDist, const double& shadowNearPlane, const double shadowFarPlane, RenderCamera* outShadowRenderCamera);

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
    std::shared_ptr<RenderPass> m_shadowRenderPass;
    std::shared_ptr<DescriptorSetLayout> m_shadowRenderPassDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_lightingRenderPassDescriptorSetLayout;

    std::shared_ptr<ComputePipeline> m_vsmBlurComputePipeline;
//    std::shared_ptr<DescriptorSetLayout> m_vsmBlurComputeDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_vsmBlurXComputeDescriptorSetLayout;
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
    std::deque<ShadowMap*> m_inactiveShadowMaps;

    std::vector<GPULight> m_lightBufferData;
    std::vector<GPUShadowMap> m_shadowMapBufferData;
    std::vector<GPUCamera> m_shadowCameraInfoBufferData;

    uint32_t m_numLightEntities;
};


#endif //WORLDENGINE_LIGHTRENDERER_H
