#ifndef WORLDENGINE_HISTOGRAMRENDERER_H
#define WORLDENGINE_HISTOGRAMRENDERER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/event/GraphicsEvents.h"

struct RecreateSwapchainEvent;

class DescriptorSetLayout;
class GraphicsPipeline;
class DescriptorSet;
class Framebuffer;
class ImageView;
class Sampler;
class Image2D;
class Buffer;
class Mesh;

class HistogramRenderer {
private:
    struct HistogramPushConstantData {
        glm::uvec2 resolution;
        float maxBrightness;
        uint32_t binCount;
        uint32_t channel;
    };

    struct RenderResources {
        Framebuffer* framebuffer = nullptr;
        ImageView* frameImageView = nullptr;
        Image2D* frameImage = nullptr;
        DescriptorSet* descriptorSet = nullptr;
        bool recreateFramebuffer = true;
        bool frameTextureChanged = true;
        std::shared_ptr<Mesh> vertexScatterMesh;
    };

public:
    HistogramRenderer();

    ~HistogramRenderer();

    bool init();

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    uint32_t getBinCount() const;

    void setBinCount(const uint32_t& binCount);

    uint32_t getDownsampleFactor() const;

    void setDownsampleFactor(const uint32_t& downsampleFactor);

    ImageView* getHistogramImageView() const;

    const std::shared_ptr<Sampler>& getSampler() const;

private:
    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createFramebuffer(RenderResources* resource);

    bool createGraphicsPipeline();

    bool createRenderPass();

    void createVertexScatterMesh();

private:
    SharedResource<RenderPass> m_renderPass;
    GraphicsPipeline* m_histogramGraphicsPipeline;
    SharedResource<DescriptorSetLayout> m_descriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<Sampler> m_sampler;
    std::shared_ptr<Mesh> m_vertexScatterMesh;
    bool m_recreateVertexScatterMesh;
    glm::uvec2 m_resolution;
    uint32_t m_downsampleFactor;
    uint32_t m_binCount;
};


#endif //WORLDENGINE_HISTOGRAMRENDERER_H
