#ifndef WORLDENGINE_POSTPROCESSRENDERER_H
#define WORLDENGINE_POSTPROCESSRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/scene/event/Events.h"

class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class GraphicsPipeline;
class Sampler;
class Image2D;
class ImageView;
class Framebuffer;

class PostProcessRenderer {
private:
    struct PostProcessUniformData {
        int32_t currentFrameIndex;
        int32_t previousFrameIndex;
        float taaHistoryFactor;
    };


//    struct FrameImage {
//        Image2D* image = nullptr;
//        ImageView* imageView = nullptr;
//        Framebuffer* framebuffer = nullptr;
//        bool rendered = false;
//    };

    struct RenderResources {
        Buffer* uniformBuffer = nullptr;
        DescriptorSet* descriptorSet = nullptr;
//        FrameImage frameImage;
    };

public:
    PostProcessRenderer();

    ~PostProcessRenderer();

    bool init();

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    const float& getTaaHistoryFactor() const;

    void setTaaHistoryFactor(const float& taaHistoryFactor);

private:
    void recreateSwapchain(const RecreateSwapchainEvent& event);

//    bool createFramebuffer(FrameImage* frameImage);

    bool createGraphicsPipeline();

//    bool createRenderPass();

private:
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
    FrameResource<RenderResources> m_resources;
    std::vector<ImageView*> m_frameTextureImageViews;
    std::shared_ptr<DescriptorSetLayout> m_postProcessingDescriptorSetLayout;
    Sampler* m_frameSampler;
    PostProcessUniformData m_uniformData;
    std::unordered_map<ImageView*, int32_t> m_frameIndices;
//    FrameImage frameImage;
};

#endif //WORLDENGINE_POSTPROCESSRENDERER_H
