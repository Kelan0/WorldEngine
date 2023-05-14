#ifndef WORLDENGINE_DEFERREDRENDERER_H
#define WORLDENGINE_DEFERREDRENDERER_H

#include "core/core.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/scene/bound/Frustum.h"

class GraphicsPipeline;
class Image2D;
class ImageView;
class Sampler;
class RenderPass;
class Framebuffer;
class DescriptorSetLayout;
class DescriptorSet;
class Buffer;
struct RecreateSwapchainEvent;

enum DeferredAttachmentType {
    Attachment_AlbedoRGB_Roughness = 0,
    Attachment_NormalXYZ_Metallic = 1,
    Attachment_EmissionRGB_AO = 2,
    Attachment_VelocityXY = 3,
    Attachment_Depth = 4,
    Attachment_LightingRGB = 5,
    NumAttachments = 6,
};

class DeferredRenderer {
private:
    struct GeometryPassUniformData {
        GPUCamera prevCamera;
        GPUCamera camera;
        glm::vec2 taaPreviousJitterOffset;
        glm::vec2 taaCurrentJitterOffset;
    };
    struct LightingPassUniformData {
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 viewProjectionMatrix;
        glm::mat4 invViewMatrix;
        glm::mat4 invProjectionMatrix;
        glm::mat4 invViewProjectionMatrix;
        glm::mat4 cameraRays;
        glm::uvec2 resolution;
        bool showDebugShadowCascades;
        uint32_t debugShadowCascadeLightIndex;
        float debugShadowCascadeOpacity;
    };

    struct FrameImages {
        std::array<Image2D*, NumAttachments> images = {};
        std::array<ImageView*, NumAttachments> imageViews = {};
        Framebuffer* framebuffer = nullptr;
        bool rendered = false;
    };

    struct RenderResources {
        FrameImages frame;
        DescriptorSet* globalDescriptorSet = nullptr;
        Buffer* cameraInfoBuffer = nullptr;
        Buffer* lightingPassUniformBuffer = nullptr;
        DescriptorSet* lightingPassDescriptorSet = nullptr;
        bool updateDescriptorSet = true;
    };

public:
    explicit DeferredRenderer();

    ~DeferredRenderer();

    bool init();

    void preRender(double dt);

    void renderGeometryPass(double dt, const vk::CommandBuffer& commandBuffer, const RenderCamera* renderCamera, const Frustum* frustum);

    void renderLightingPass(double dt, const vk::CommandBuffer& commandBuffer, const RenderCamera* renderCamera, const Frustum* frustum);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    void beginGeometrySubpass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    void beginLightingSubpass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    void presentDirect(const vk::CommandBuffer& commandBuffer);

    bool hasPreviousFrame() const;

    ImageView* getAlbedoImageView() const;

    ImageView* getNormalImageView() const;

    ImageView* getEmissionImageView() const;

    ImageView* getVelocityImageView() const;

    ImageView* getDepthImageView() const;

    ImageView* getOutputFrameImageView() const;

    ImageView* getPreviousAlbedoImageView() const;

    ImageView* getPreviousNormalImageView() const;

    ImageView* getPreviousEmissionImageView() const;

    ImageView* getPreviousVelocityImageView() const;

    ImageView* getPreviousDepthImageView() const;

    ImageView* getPreviousOutputFrameImageView() const;

    vk::Format getAttachmentFormat(uint32_t attachment) const;

    vk::Format getOutputColourFormat() const;

    const std::shared_ptr<Sampler>& getDepthSampler() const;

private:
    void recreateSwapchain(RecreateSwapchainEvent* event);

    bool createFramebuffer(FrameImages* frame);

    bool createGeometryGraphicsPipeline();

    bool createLightingGraphicsPipeline();

    bool createRenderPass();

    void swapFrame();

private:
    SharedResource<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_geometryGraphicsPipeline;
    std::shared_ptr<GraphicsPipeline> m_lightingGraphicsPipeline;
    FrameResource<RenderResources> m_resources;
    SharedResource<DescriptorSetLayout> m_globalDescriptorSetLayout;
    SharedResource<DescriptorSetLayout> m_lightingDescriptorSetLayout;
    std::shared_ptr<Sampler> m_attachmentSampler;
    std::shared_ptr<Sampler> m_depthSampler;
    std::unordered_map<ImageView*, int32_t> m_frameIndices;
    FrameImages m_previousFrame;
};



#endif //WORLDENGINE_DEFERREDRENDERER_H
