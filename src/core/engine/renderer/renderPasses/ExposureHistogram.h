#ifndef WORLDENGINE_EXPOSUREHISTOGRAM_H
#define WORLDENGINE_EXPOSUREHISTOGRAM_H

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

class ExposureHistogram {
private:
    struct RenderResources {
        DescriptorSet* descriptorSet = nullptr;
        Buffer* histogramBuffer;
        bool frameTextureChanged = true;
        bool prevHistogramBufferChanged = true;
    };

    struct HistogramStorageBufferHeader {
        uint32_t binCount;
        float offset;
        float scale;
        float averageLuminance;
        uint32_t maxValue;
        uint32_t sumValue;
        float prevExposure;
        float exposure;
    };

public:
    ExposureHistogram();

    ~ExposureHistogram();

    bool init();

    void update(const double& dt, const vk::CommandBuffer& commandBuffer);

    uint32_t getBinCount() const;

    void setBinCount(const uint32_t& binCount);

    uint32_t getDownsampleFactor() const;

    void setDownsampleFactor(const uint32_t& downsampleFactor);

    float getOffset() const;

    void setOffset(const float& offset);

    float getScale() const;

    void setScale(const float& scale);

    float getLowPercent() const;

    void setLowPercent(const float& lowPercent);

    float getHighPercent() const;

    void setHighPercent(const float& highPercent);

    float getExposureSpeedUp() const;

    void setExposureSpeedUp(const float& exposureSpeedUp);

    float getExposureSpeedDown() const;

    void setExposureSpeedDown(const float& exposureSpeedDown);

    float getExposureCompensation() const;

    void setExposureCompensation(const float& exposureCompensation);

    Buffer* getHistogramBuffer() const;

private:
    void readback(const vk::CommandBuffer& commandBuffer);

    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool updateHistogramBuffer(RenderResources* resource);

    bool createHistogramClearComputePipeline();

    bool createHistogramAccumulationComputePipeline();

    bool createHistogramAverageComputePipeline();

private:
    SharedResource<RenderPass> m_renderPass;
    SharedResource<DescriptorSetLayout> m_descriptorSetLayout;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<Sampler> m_inputFrameSampler;

    ComputePipeline* m_histogramClearComputePipeline;
    ComputePipeline* m_histogramAccumulationComputePipeline;
    ComputePipeline* m_histogramAverageComputePipeline;

    bool m_readbackNextFrame;
    HistogramStorageBufferHeader m_readbackHeader;
    std::vector<uint32_t> m_readbackData;
    Buffer* m_readbackBuffer;
    glm::uvec2 m_resolution;
    uint32_t m_downsampleFactor;
    uint32_t m_binCount;
    float m_offset;
    float m_scale;
    float m_lowPercent;
    float m_highPercent;
    float m_exposureSpeedUp;
    float m_exposureSpeedDown;
    float m_exposureCompensation;
    glm::bvec4 m_enabledChannels;
};


#endif //WORLDENGINE_EXPOSUREHISTOGRAM_H