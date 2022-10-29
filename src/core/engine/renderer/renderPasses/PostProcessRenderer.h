#ifndef WORLDENGINE_POSTPROCESSRENDERER_H
#define WORLDENGINE_POSTPROCESSRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"

class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class GraphicsPipeline;
class Sampler;
class Image2D;
class ImageView;
class Framebuffer;
struct RecreateSwapchainEvent;

class PostProcessRenderer {
private:
    struct PostProcessUniformData {
        int32_t temp;
    };

    struct BloomBlurUniformData {
        glm::vec2 texelSize;
        float filterRadius;
        uint32_t textureIndex;
    };

    struct RenderResources {
        Buffer* postProcessUniformBuffer = nullptr;
        DescriptorSet* postProcessDescriptorSet = nullptr;
        Buffer* bloomBlurUniformBuffer = nullptr;
        DescriptorSet* bloomBlurInputDescriptorSet = nullptr;
        std::vector<DescriptorSet*> bloomBlurDescriptorSets;
        std::vector<Framebuffer*> bloomBlurMipFramebuffers;
        std::vector<ImageView*> bloomBlurMipImageViews;
        Image2D* bloomBlurImage = nullptr;
        bool updateInputImage = true;
    };

public:
    PostProcessRenderer();

    ~PostProcessRenderer();

    bool init();

    void renderBloomBlur(const double& dt, const vk::CommandBuffer& commandBuffer);

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

private:
    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createBloomBlurFramebuffer(RenderResources* resources);

    bool createDownsampleGraphicsPipeline();

    bool createUpsampleGraphicsPipeline();

    bool createPostProcessGraphicsPipeline();

    bool createBloomBlurRenderPass();

private:
    std::shared_ptr<RenderPass> m_bloomBlurRenderPass;
    std::shared_ptr<GraphicsPipeline> m_downsampleGraphicsPipeline;
    std::shared_ptr<GraphicsPipeline> m_upsampleGraphicsPipeline;
    std::shared_ptr<GraphicsPipeline> m_postProcessGraphicsPipeline;
    std::shared_ptr<DescriptorSetLayout> m_postProcessDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_bloomBlurDescriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<Sampler> m_frameSampler;
    PostProcessUniformData m_postProcessUniformData;
    uint32_t m_bloomBlurImageMipLevels;
    uint32_t m_bloomBlurImageMaxMipLevels;
};

#endif //WORLDENGINE_POSTPROCESSRENDERER_H
