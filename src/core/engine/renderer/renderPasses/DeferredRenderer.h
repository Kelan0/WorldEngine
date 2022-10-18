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

    void render(double dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    void beginRenderPass(const vk::CommandBuffer& commandBuffer, const vk::SubpassContents& subpassContents);

    [[nodiscard]] std::shared_ptr<RenderPass> getRenderPass() const;

    [[nodiscard]] GraphicsPipeline* getGraphicsPipeline() const;

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
        bool showDebugShadowCascades;
        uint32_t debugShadowCascadeLightIndex;
        float debugShadowCascadeOpacity;
    };

    struct RenderResources {
        Buffer* uniformBuffer;
        DescriptorSet* lightingDescriptorSet;
        Framebuffer* framebuffer;
        Image2D* frameImage;
        ImageView* frameImageView;
    };

public:
    explicit DeferredLightingRenderPass(DeferredGeometryRenderPass* geometryPass);

    ~DeferredLightingRenderPass();

    bool init();

    void renderScreen(double dt);

    GraphicsPipeline* getGraphicsPipeline() const;

private:
    void recreateSwapchain(const RecreateSwapchainEvent& event);

    bool createFramebuffer(RenderResources* resources);

    bool createGraphicsPipeline();

    bool createRenderPass();

private:
    DeferredGeometryRenderPass* m_geometryPass;
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
    FrameResource<RenderResources> m_resources;
    std::shared_ptr<DescriptorSetLayout> m_lightingDescriptorSetLayout;
    std::array<Sampler*, NumAttachments> m_attachmentSamplers{};
    RenderCamera m_renderCamera;
};



#endif //WORLDENGINE_DEFERREDRENDERER_H
