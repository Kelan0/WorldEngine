#ifndef WORLDENGINE_REPROJECTIONRENDERER_H
#define WORLDENGINE_REPROJECTIONRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/GraphicsResource.h"

class RenderPass;
class GraphicsPipeline;
class Framebuffer;
class Image2D;
class ImageView;
class Sampler;
class Buffer;
class DescriptorSetLayout;
class DescriptorSet;
struct RecreateSwapchainEvent;

class ReprojectionRenderer {
public:
    enum ColourClippingMode {
        ColourClippingMode_None = 0,
        ColourClippingMode_Fast = 1,
        ColourClippingMode_Accurate = 2,
    };

private:
    struct ReprojectionUniformData {
        glm::vec2 resolution;
        glm::vec2 taaCurrentJitterOffset;
        glm::vec2 taaPreviousJitterOffset;
        float taaHistoryFadeFactor;
        uint32_t useCatmullRomFilter;
        uint32_t colourClippingMode;
        uint32_t useMitchellFilter;
        float mitchellB;
        float mitchellC;
        uint32_t taaEnabled;
        uint32_t hasPreviousFrame;
    };

    struct FrameImages {
        Image2D* image = nullptr;
        ImageView* imageView = nullptr;
        Framebuffer* framebuffer = nullptr;
        bool rendered = false;
    };

    struct RenderResources {
        DescriptorSet* reprojectionDescriptorSet = nullptr;
        Buffer* reprojectionUniformBuffer = nullptr;
        FrameImages frame;
        bool updateDescriptorSet;
    };

public:
    explicit ReprojectionRenderer();

    ~ReprojectionRenderer();

    bool init();

    void preRender(double dt);

    void render(double dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    ImageView* getOutputFrameImageView() const;

    ImageView* getPreviousFrameImageView() const;

    bool hasPreviousFrame() const;

    float getTaaHistoryFactor() const;

    void setTaaHistoryFactor(float taaHistoryFactor);

    bool getTaaUseCatmullRomFilter() const;

    void setTaaUseCatmullRomFilter(bool useCatmullRomFilter);

    ColourClippingMode getTaaColourClippingMode() const;

    void setTaaColourClippingMode(ColourClippingMode colourClippingMode);

    glm::vec2 getTaaMitchellFilterCoefficients() const;

    void setTaaMitchellFilterCoefficients(float B, float C);

    bool getTaaUseMitchellFilter() const;

    void setTaaUseMitchellFilter(bool useMitchellFilter);

    bool isTaaEnabled() const;

    void setTaaEnabled(bool taaEnabled);

    const glm::vec2& getTaaPreviousJitterOffset() const;

    const glm::vec2& getTaaCurrentJitterOffset() const;

    void setTaaJitterSampleCount(uint32_t sampleCount);

private:
    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createFramebuffer(FrameImages* frame);

    bool createReprojectionGraphicsPipeline();

    bool createRenderPass();

    void swapFrames();

private:
    SharedResource<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_reprojectionGraphicsPipeline;
    SharedResource<DescriptorSetLayout> m_reprojectionDescriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::unordered_map<ImageView*, int32_t> m_frameIndices;
    std::shared_ptr<Sampler> m_frameSampler;
    ReprojectionUniformData m_uniformData;
    FrameImages m_previousFrame;
    std::vector<glm::vec2> m_haltonSequence;
};


#endif //WORLDENGINE_REPROJECTIONRENDERER_H
