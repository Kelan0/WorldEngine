#ifndef WORLDENGINE_REPROJECTIONRENDERER_H
#define WORLDENGINE_REPROJECTIONRENDERER_H

#include "core/core.h"
#include "core/engine/scene/event/Events.h"
#include "core/graphics/FrameResource.h"

class RenderPass;
class GraphicsPipeline;
class Framebuffer;
class Image2D;
class ImageView;
class Sampler;
class Buffer;
class DescriptorSetLayout;
class DescriptorSet;

class ReprojectionRenderer {
private:
    struct ReprojectionUniformData {
        glm::uvec2 resolution;
        float taaHistoryFactor;
        bool taaUseFullKernel;
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

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    ImageView* getOutputFrameImageView() const;

    ImageView* getPreviousFrameImageView() const;

    bool hasPreviousFrame() const;

    const float& getTaaHistoryFactor() const;

    void setTaaHistoryFactor(const float& taaHistoryFactor);

private:
    void recreateSwapchain(const RecreateSwapchainEvent& event);

    bool createFramebuffer(FrameImages* frame);

    bool createReprojectionGraphicsPipeline();

    bool createRenderPass();

    void swapFrames();

private:
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_reprojectionGraphicsPipeline;
    std::shared_ptr<DescriptorSetLayout> m_reprojectionDescriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::unordered_map<ImageView*, int32_t> m_frameIndices;
    std::shared_ptr<Sampler> m_frameSampler;
    ReprojectionUniformData m_uniformData;
    FrameImages m_previousFrame;
};


#endif //WORLDENGINE_REPROJECTIONRENDERER_H
