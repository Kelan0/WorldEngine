
#ifndef WORLDENGINE_LIGHTRENDERER_H
#define WORLDENGINE_LIGHTRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/light/Light.h"

class DirectionShadowMap;
class GraphicsPipeline;
class ComputePipeline;
class RenderPass;
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class Texture;
class ImageView;

#define MAX_SHADOW_MAPS 1024

class LightRenderer {
public:
    LightRenderer();

    ~LightRenderer();

    bool init();

    void render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    const std::shared_ptr<RenderPass>& getRenderPass() const;

    DirectionShadowMap* getTestShadowMap() const;

    const uint32_t& getMaxVisibleShadowMaps() const;

    const std::shared_ptr<Texture>& getEmptyShadowMap() const;

    const std::shared_ptr<DescriptorSetLayout>& getLightingRenderPassDescriptorSetLayout() const;

    DescriptorSet* getLightingRenderPassDescriptorSet() const;

private:
    void initEmptyShadowMap();

    void updateCameraInfoBuffer(const size_t& maxShadowLights);

    void updateLightInfoBuffer(const size_t& maxLights);

    void updateShadowMapInfoBuffer(const size_t& maxShadowLights);

    void blurImage(const vk::CommandBuffer& commandBuffer, const Sampler* sampler, const ImageView* srcImage, const glm::uvec2& srcSize, const ImageView* dstImage, const glm::uvec2& dstSize, const glm::vec2& blurRadius);

    void calculateShadowRenderCamera(const RenderCamera* viewerRenderCamera, const DirectionShadowMap* shadowMap, const double& nearDistance, const double& farDistance, RenderCamera* outShadowRenderCamera);

private:
    DirectionShadowMap* m_testDirectionShadowMap;

    std::shared_ptr<GraphicsPipeline> m_shadowGraphicsPipeline;
    std::shared_ptr<RenderPass> m_shadowRenderPass;
    std::shared_ptr<DescriptorSetLayout> m_shadowRenderPassDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_lightingRenderPassDescriptorSetLayout;

    std::shared_ptr<ComputePipeline> m_vsmBlurComputePipeline;
    std::shared_ptr<DescriptorSetLayout> m_vsmBlurComputeDescriptorSetLayout;
    DescriptorSet* m_vsmBlurXComputeDescriptorSet;
    DescriptorSet* m_vsmBlurYComputeDescriptorSet;

    struct ShadowRenderPassResources {
        DescriptorSet* descriptorSet;
        Buffer* cameraInfoBuffer;
    };

    struct LightingRenderPassResources {
        DescriptorSet* descriptorSet;
        Buffer* lightInfoBuffer;
        Buffer* shadowMapBuffer;
    };

    FrameResource<ShadowRenderPassResources> m_shadowRenderPassResources;
    FrameResource<LightingRenderPassResources> m_lightingRenderPassResources;

    std::shared_ptr<Image2D> m_emptyShadowMapImage;
    std::shared_ptr<Texture> m_emptyShadowMap;

    std::shared_ptr<Sampler> m_vsmShadowMapSampler;

    Image2D* m_vsmBlurIntermediateImage;
    ImageView* m_vsmBlurIntermediateImageView;

    uint32_t m_maxVisibleShadowMaps;
};


#endif //WORLDENGINE_LIGHTRENDERER_H
