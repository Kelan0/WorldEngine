#ifndef WORLDENGINE_FRAMEBUFFER_H
#define WORLDENGINE_FRAMEBUFFER_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

class ImageView;

class RenderPass;

struct FramebufferConfiguration {
    WeakResource<vkr::Device> device;
    vk::RenderPass renderPass = nullptr;
    std::vector<vk::ImageView> attachments;
    uint32_t width;
    uint32_t height;
    uint32_t layers = 1;

    void setRenderPass(const vk::RenderPass& renderPass);

    void setRenderPass(const RenderPass* renderPass);

    void addAttachment(const vk::ImageView& imageView);

    void addAttachment(const ImageView* imageView);

    void setAttachments(const vk::ArrayProxy<vk::ImageView>& imageViews);

    void setAttachments(const vk::ArrayProxy<ImageView*>& imageViews);

    void setAttachment(uint32_t index, const vk::ImageView& imageView);

    void setAttachment(uint32_t index, const ImageView* imageView);

    void setSize(uint32_t width, uint32_t height);

    void setSize(const glm::uvec2& size);

    void setSize(const vk::Extent2D& size);
};

class Framebuffer : public GraphicsResource {
private:
    Framebuffer(const WeakResource<vkr::Device>& device, const vk::Framebuffer& framebuffer, const glm::uvec2& resolution, const std::string& name);

public:
    ~Framebuffer() override;

    static Framebuffer* create(const FramebufferConfiguration& framebufferConfiguration, const std::string& name);

    const vk::Framebuffer& getFramebuffer() const;

    const glm::uvec2& getResolution() const;

    uint32_t getWidth() const;

    uint32_t getHeight() const;

private:
    vk::Framebuffer m_framebuffer;
    glm::uvec2 m_resolution;
};


#endif //WORLDENGINE_FRAMEBUFFER_H
