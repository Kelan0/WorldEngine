#ifndef WORLDENGINE_SHADOWMAP_H
#define WORLDENGINE_SHADOWMAP_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/engine/scene/event/Events.h"

class Image2D;
class Texture;
class Framebuffer;
class GraphicsPipeline;
class RenderPass;
class Buffer;
class DescriptorSet;
class DescriptorSetLayout;

enum ShadowMapType {
    ShadowMapType_Simple = 0,
    ShadowMapType_Variance = 1,
};

struct GPUShadowMap {
    glm::mat4 viewProjectionMatrix;
};


class ShadowMap {
    friend class LightRenderer;
public:
    ShadowMap(const uint32_t& width, const uint32_t& height);

    explicit ShadowMap(const glm::uvec2& resolution);

    ~ShadowMap();

    void update();

    void begin(const vk::CommandBuffer& commandBuffer, const RenderPass* renderPass);

    void render(const vk::CommandBuffer& commandBuffer);

    [[nodiscard]] const glm::uvec2& getResolution() const;

    [[nodiscard]] const ImageView* getShadowVarianceImageView() const;

    [[nodiscard]] const glm::mat4& getViewProjectionMatrix() const;

private:
    glm::uvec2 m_resolution;
    ImageView* m_shadowDepthImageView;
    ImageView* m_shadowVarianceImageView;
    Image2D* m_shadowDepthImage;
    Image2D* m_shadowVarianceImage;
    Framebuffer* m_shadowMapFramebuffer;
    glm::mat4 m_viewProjectionMatrix;
};


#endif //WORLDENGINE_SHADOWMAP_H
