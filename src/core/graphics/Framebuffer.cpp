#include "core/graphics/Framebuffer.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Engine.h"
#include "core/util/Logger.h"


void FramebufferConfiguration::setRenderPass(const vk::RenderPass& p_renderPass) {
    assert(p_renderPass);
    renderPass = p_renderPass;
}

void FramebufferConfiguration::setRenderPass(const RenderPass* p_renderPass) {
    assert(p_renderPass != nullptr);
    setRenderPass(p_renderPass->getRenderPass());
}

void FramebufferConfiguration::addAttachment(const vk::ImageView& imageView) {
    assert(imageView);
    attachments.emplace_back(imageView);
}

void FramebufferConfiguration::addAttachment(const ImageView* imageView) {
    assert(imageView != nullptr);
    addAttachment(imageView->getImageView());
}

void FramebufferConfiguration::setAttachments(const vk::ArrayProxy<vk::ImageView>& imageViews) {
    attachments.clear();
    for (const auto& imageView : imageViews)
        addAttachment(imageView);
}

void FramebufferConfiguration::setAttachments(const vk::ArrayProxy<ImageView*>& imageViews) {
    attachments.clear();
    for (const auto& imageView : imageViews)
        addAttachment(imageView);
}

void FramebufferConfiguration::setAttachment(uint32_t index, const vk::ImageView& imageView) {
    assert(index <= attachments.size());
    if (index == attachments.size()) {
        addAttachment(imageView);
    } else {
        attachments[index] = imageView;
    }
}

void FramebufferConfiguration::setAttachment(uint32_t index, const ImageView* imageView) {
    setAttachment(index, imageView->getImageView());
}

void FramebufferConfiguration::setSize(uint32_t p_width, uint32_t p_height) {
    width = p_width;
    height = p_height;
}

void FramebufferConfiguration::setSize(const glm::uvec2& size) {
    setSize(size.x, size.y);
}

void FramebufferConfiguration::setSize(const vk::Extent2D& size) {
    setSize(size.width, size.height);
}


Framebuffer::Framebuffer(const WeakResource<vkr::Device>& device, const vk::Framebuffer& framebuffer, const glm::uvec2& resolution, const std::string& name):
        GraphicsResource(ResourceType_Framebuffer, device, name),
        m_framebuffer(framebuffer),
        m_resolution(resolution) {
}

Framebuffer::~Framebuffer() {
    (**m_device).destroyFramebuffer(m_framebuffer);
    m_framebuffer = nullptr;
}

Framebuffer* Framebuffer::create(const FramebufferConfiguration& framebufferConfiguration, const std::string& name) {
    const vk::Device& device = **framebufferConfiguration.device.lock(name);

    if (!framebufferConfiguration.renderPass) {
        LOG_ERROR("Unable to create Framebuffer: RenderPass is NULL");
        return nullptr;
    }
    if (framebufferConfiguration.attachments.empty()) {
        LOG_ERROR("Unable to create Framebuffer: No attachments");
        return nullptr;
    }
    if (framebufferConfiguration.width == 0 || framebufferConfiguration.height == 0) {
        LOG_ERROR("Unable to create framebuffer: Zero size dimensions");
        return nullptr;
    }
    if (framebufferConfiguration.layers == 0) {
        LOG_ERROR("Unable to create framebuffer: Must have ate least 1 layer");
        return nullptr;
    }

    vk::FramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.setRenderPass(framebufferConfiguration.renderPass);
    framebufferCreateInfo.setAttachmentCount((uint32_t)framebufferConfiguration.attachments.size());
    framebufferCreateInfo.setPAttachments(framebufferConfiguration.attachments.data());
    framebufferCreateInfo.setWidth(framebufferConfiguration.width);
    framebufferCreateInfo.setHeight(framebufferConfiguration.height);
    framebufferCreateInfo.setLayers(framebufferConfiguration.layers);

    vk::Framebuffer framebuffer = nullptr;
    vk::Result result = device.createFramebuffer(&framebufferCreateInfo, nullptr, &framebuffer);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create Vulkan Framebuffer: %s", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkFramebuffer)framebuffer, vk::ObjectType::eFramebuffer, name);

    return new Framebuffer(framebufferConfiguration.device, framebuffer, glm::uvec2(framebufferConfiguration.width, framebufferConfiguration.height), name);
}

const vk::Framebuffer& Framebuffer::getFramebuffer() const {
    return m_framebuffer;
}

const glm::uvec2& Framebuffer::getResolution() const {
    return m_resolution;
}

uint32_t Framebuffer::getWidth() const {
    return m_resolution.x;
}

uint32_t Framebuffer::getHeight() const {
    return m_resolution.y;
}