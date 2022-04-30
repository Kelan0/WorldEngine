#ifndef WORLDENGINE_FRAMEBUFFER_H
#define WORLDENGINE_FRAMEBUFFER_H

#include "core/core.h"

class ImageView2D;
class RenderPass;

struct FramebufferConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::RenderPass renderPass = VK_NULL_HANDLE;
    std::vector<vk::ImageView> attachments;
    uint32_t width;
    uint32_t height;
    uint32_t layers = 1;

    void setRenderPass(const vk::RenderPass& renderPass);

    void setRenderPass(const RenderPass* renderPass);

    void addAttachment(const vk::ImageView& imageView);

    void addAttachment(const ImageView2D* imageView);

    void setAttachments(const vk::ArrayProxy<vk::ImageView>& imageViews);

    void setAttachments(const vk::ArrayProxy<ImageView2D*>& imageViews);

    void setSize(const uint32_t& width, const uint32_t& height);

    void setSize(const glm::uvec2& size);

    void setSize(const vk::Extent2D& size);
};

class Framebuffer {
private:
    Framebuffer(std::weak_ptr<vkr::Device> device, const vk::Framebuffer& framebuffer, const glm::uvec2& resolution);

public:
    ~Framebuffer();

    static Framebuffer* create(const FramebufferConfiguration& framebufferConfiguration);

    const vk::Framebuffer& getFramebuffer() const;

    const glm::uvec2& getResolution() const;

    const uint32_t& getWidth() const;

    const uint32_t& getHeight() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Framebuffer m_framebuffer;
    glm::uvec2 m_resolution;
};


#endif //WORLDENGINE_FRAMEBUFFER_H
