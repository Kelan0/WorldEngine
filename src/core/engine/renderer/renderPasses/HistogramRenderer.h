#ifndef WORLDENGINE_HISTOGRAMRENDERER_H
#define WORLDENGINE_HISTOGRAMRENDERER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/event/GraphicsEvents.h"

struct RecreateSwapchainEvent;

class DescriptorSetLayout;
class GraphicsPipeline;
class ComputePipeline;
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
        float offset;
        float scale;
    };

    struct RenderResources {
        DescriptorSet* descriptorSet = nullptr;
        Framebuffer* vertexScatterFramebuffer = nullptr;
        ImageView* histogramImageView = nullptr;
        Image2D* histogramImage = nullptr;
        std::shared_ptr<Mesh> vertexScatterMesh;
        Buffer* histogramBuffer;
        bool frameTextureChanged = true;
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

    float getOffset() const;

    void setOffset(const float& offset);

    float getScale() const;

    void setScale(const float& scale);

    float getMinBrightness() const;

    void setMinBrightness(const float& minBrightness);

    float getMaxBrightness() const;

    void setMaxBrightness(const float& maxBrightness);

    glm::bvec4 getEnabledChannels() const;

    void setEnabledChannels(const glm::bvec4& enabledChannels);

    Buffer* getHistogramBuffer() const;

    ImageView* getHistogramImageView() const;

    const std::shared_ptr<Sampler>& getSampler() const;

private:
    void renderVertexScatterHistogram(const vk::CommandBuffer& commandBuffer);

    void renderComputeHistogram(const vk::CommandBuffer& commandBuffer);

    void readback(const vk::CommandBuffer& commandBuffer);

    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createVertexScatterFramebuffer(RenderResources* resource);

    void updateHistogramBuffer(RenderResources* resource);

    bool createVertexScatterGraphicsPipeline();

    bool createHistogramClearComputePipeline();

    bool createHistogramAccumulationComputePipeline();

    bool createVertexScatterRenderPass();

    void createVertexScatterMesh();

    void updateReadbackBuffer();

private:
    SharedResource<RenderPass> m_renderPass;
    SharedResource<DescriptorSetLayout> m_descriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<Sampler> m_inputFrameSampler;

    GraphicsPipeline* m_histogramVertexScatterGraphicsPipeline;
    std::shared_ptr<Mesh> m_vertexScatterMesh;
    bool m_recreateVertexScatterMesh;

    ComputePipeline* m_histogramClearComputePipeline;
    ComputePipeline* m_histogramAccumulationComputePipeline;

    bool m_readbackNextFrame;
    std::vector<glm::vec4> m_readbackData;
    Buffer* m_readbackBuffer;
    glm::uvec2 m_resolution;
    uint32_t m_downsampleFactor;
    uint32_t m_binCount;
    float m_offset;
    float m_scale;
    float m_minBrightness;
    float m_maxBrightness;
    glm::bvec4 m_enabledChannels;
};


#endif //WORLDENGINE_HISTOGRAMRENDERER_H
