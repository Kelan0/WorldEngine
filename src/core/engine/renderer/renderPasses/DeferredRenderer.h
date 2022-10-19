#ifndef WORLDENGINE_DEFERREDRENDERER_H
#define WORLDENGINE_DEFERREDRENDERER_H

#include "core/core.h"
#include "core/engine/scene/event/Events.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/graphics/FrameResource.h"

class GraphicsPipeline;
class Image2D;
class ImageView;
class Sampler;
class RenderPass;
class Framebuffer;
class DescriptorSetLayout;
class DescriptorSet;
class Buffer;

enum DeferredAttachmentType {
    Attachment_AlbedoRGB_Roughness = 0,
    Attachment_NormalXYZ_Metallic = 1,
    Attachment_EmissionRGB_AO = 2,
    Attachment_VelocityXY = 3,
    Attachment_Depth = 4,
    NumAttachments = 5
};


class DeferredGeometryRenderPass {
    friend class DeferredLightingRenderPass;
private:
    struct GeometryPassUniformData {
        GPUCamera prevCamera;
        GPUCamera camera;
        glm::vec2 taaJitterOffset;
    };

    struct RenderResources {
        std::array<Image2D*, NumAttachments> images;
        std::array<ImageView*, NumAttachments> imageViews;
        Framebuffer* framebuffer;
        DescriptorSet* globalDescriptorSet;
        Buffer* cameraInfoBuffer;
    };

public:
    DeferredGeometryRenderPass();

    ~DeferredGeometryRenderPass();

    bool init();

    void render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    [[nodiscard]] std::shared_ptr<RenderPass> getRenderPass() const;

    [[nodiscard]] ImageView* getAlbedoImageView() const;

    [[nodiscard]] ImageView* getNormalImageView() const;

    [[nodiscard]] ImageView* getEmissionImageView() const;

    [[nodiscard]] ImageView* getVelocityImageView() const;

    [[nodiscard]] ImageView* getDepthImageView() const;

    [[nodiscard]] vk::Format getAttachmentFormat(const uint32_t& attachment) const;

private:
    void recreateSwapchain(const RecreateSwapchainEvent& event);

    bool createFramebuffer(RenderResources* resources);

    bool createGraphicsPipeline();

    bool createRenderPass();

private:
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<DescriptorSetLayout> m_globalDescriptorSetLayout;
    std::vector<glm::vec2> m_haltonSequence;
    uint32_t m_frameIndex;
};




class DeferredLightingRenderPass {
private:
    struct LightingPassUniformData {
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 viewProjectionMatrix;
        glm::mat4 invViewMatrix;
        glm::mat4 invProjectionMatrix;
        glm::mat4 invViewProjectionMatrix;
        glm::mat4 cameraRays;
        glm::uvec2 resolution;
        int32_t currentFrameIndex;
        int32_t previousFrameIndex;
        bool showDebugShadowCascades;
        uint32_t debugShadowCascadeLightIndex;
        float debugShadowCascadeOpacity;
        float taaHistoryFactor;
        bool taaUseFullKernel;
    };

    struct FrameImage {
        Image2D* image = nullptr;
        ImageView* imageView = nullptr;
        Framebuffer* framebuffer = nullptr;
        bool rendered = false;
    };
    struct RenderResources {
        Buffer* uniformBuffer = nullptr;
        DescriptorSet* lightingDescriptorSet = nullptr;
        bool updateDescriptorSet = true;
        FrameImage frameImage;
    };

public:
    explicit DeferredLightingRenderPass(DeferredGeometryRenderPass* geometryPass);

    ~DeferredLightingRenderPass();

    bool init();

    void preRender(const double& dt);

    void render(const double& dt, const vk::CommandBuffer& commandBuffer);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    void presentDirect(const vk::CommandBuffer& commandBuffer);

    void setTaaHistoryFactor(const float& taaHistoryFactor);

    bool hasPreviousFrame() const;

    ImageView* getPreviousFrameImageView() const;

    ImageView* getCurrentFrameImageView() const;

    vk::Format getOutputColourFormat() const;

private:
    void recreateSwapchain(const RecreateSwapchainEvent& event);

    bool createFramebuffer(FrameImage* frameImage);

    bool createGraphicsPipeline();

    bool createRenderPass();

    void swapFrameImage(FrameImage* frameImage1, FrameImage* frameImage2);

private:
    DeferredGeometryRenderPass* m_geometryPass;
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<DescriptorSetLayout> m_lightingDescriptorSetLayout;
    Sampler* m_attachmentSampler;
    Sampler* m_frameSampler;
    RenderCamera m_renderCamera;
    FrameImage* m_prevFrameImage;
    std::unordered_map<ImageView*, int32_t> m_frameIndices;
    float m_taaHistoryFactor;
};



#endif //WORLDENGINE_DEFERREDRENDERER_H
