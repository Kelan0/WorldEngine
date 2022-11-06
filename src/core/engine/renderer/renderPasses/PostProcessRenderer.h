#ifndef WORLDENGINE_POSTPROCESSRENDERER_H
#define WORLDENGINE_POSTPROCESSRENDERER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
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
class ExposureHistogram;

class PostProcessRenderer {
private:
    struct PostProcessPushConstantData {
        float deltaTime;
        float time;
        float test;
    };

    struct PostProcessUniformData {
        bool bloomEnabled;
        float bloomIntensity;
        bool debugCompositeEnabled;
        float histogramOffset;
        float histogramScale;
    };

    struct BloomBlurPushConstantData {
        glm::vec2 texelSize;
        uint32_t passIndex;
        uint32_t _pad0;
    };

    struct BloomBlurUniformData {
        float filterRadius;
        float threshold;
        float softThreshold;
        float maxBrightness;
    };

    struct RenderResources {
        Buffer* postProcessUniformBuffer = nullptr;
        DescriptorSet* postProcessDescriptorSet = nullptr;
        Buffer* bloomBlurUniformBuffer = nullptr;
        DescriptorSet* bloomBlurInputDescriptorSet = nullptr;
        std::vector<DescriptorSet*> bloomBlurDescriptorSets;
        std::vector<Framebuffer*> bloomBlurMipFramebuffers;
        std::vector<ImageView*> bloomBlurMipImageViews;
        ImageView* bloomTextureImageView = nullptr;
        Image2D* bloomBlurImage = nullptr;
        bool updateInputImage = true;
        bool postProcessUniformDataChanged = true;
        bool bloomBlurUniformDataChanged = true;
        uint32_t bloomBlurIterations = 0;
    };

public:
    PostProcessRenderer();

    ~PostProcessRenderer();

    bool init();

    void updateExposure(const double& dt, const vk::CommandBuffer& commandBuffer);

    void renderBloomBlur(const double& dt, const vk::CommandBuffer& commandBuffer);

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    bool isBloomEnabled() const;

    void setBloomEnabled(const bool& bloomEnabled);

    float getBloomIntensity() const;

    void setBloomIntensity(const float& bloomIntensity);

    float getBloomBlurFilterRadius() const;

    void setBloomBlurFilterRadius(const float& bloomBlurFilterRadius);

    float getBloomThreshold() const;

    void setBloomThreshold(const float& bloomThreshold);

    float getBloomSoftThreshold() const;

    void setBloomSoftThreshold(const float& bloomSoftThreshold);

    float getBloomMaxBrightness() const;

    void setBloomMaxBrightness(const float& bloomMaxBrightness);

    uint32_t getMaxBloomBlurIterations() const;

    uint32_t getBloomBlurIterations() const;

    void setBloomBlurIterations(const uint32_t& bloomBlurIterations);

    void setTest(const float& test);

    ExposureHistogram* exposureHistogram();

private:
    void setPostProcessUniformDataChanged(const bool& didChange);

    void setBloomBlurUniformDataChanged(const bool& didChange);

    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createBloomBlurFramebuffer(RenderResources* resources);

    bool createDownsampleGraphicsPipeline();

    bool createUpsampleGraphicsPipeline();

    bool createPostProcessGraphicsPipeline();

    bool createBloomBlurRenderPass();

private:
    SharedResource<RenderPass> m_bloomBlurRenderPass;
    std::shared_ptr<GraphicsPipeline> m_downsampleGraphicsPipeline;
    std::shared_ptr<GraphicsPipeline> m_upsampleGraphicsPipeline;
    std::shared_ptr<GraphicsPipeline> m_postProcessGraphicsPipeline;
    SharedResource<DescriptorSetLayout> m_postProcessDescriptorSetLayout;
    SharedResource<DescriptorSetLayout> m_bloomBlurDescriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<Sampler> m_frameSampler;
    PostProcessUniformData m_postProcessUniformData;
    BloomBlurUniformData m_bloomBlurUniformData;
    uint32_t m_bloomBlurIterations;
    uint32_t m_bloomBlurMaxIterations;
    ExposureHistogram* m_exposureHistogram;
    float m_test;
};

#endif //WORLDENGINE_POSTPROCESSRENDERER_H
