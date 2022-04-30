#ifndef WORLDENGINE_RENDERPASS_H
#define WORLDENGINE_RENDERPASS_H

#include "core/core.h"

class Framebuffer;

struct SubpassConfiguration {
    std::vector<vk::AttachmentReference> attachmentReferences;
    std::vector<size_t> colourAttachments;
    size_t depthStencilAttachment = (size_t)(-1);
    // TODO: resolve attachments, preserve attachments, input attachments

    void addColourAttachment(const vk::AttachmentReference& attachmentReference);
    void addColourAttachment(const uint32_t& attachment, const vk::ImageLayout& imageLayout);

    void setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference);
    void setDepthStencilAttachment(const uint32_t& attachment, const vk::ImageLayout& imageLayout);
};

struct RenderPassConfiguration {
    std::weak_ptr<vkr::Device> device;
    std::vector<vk::AttachmentDescription> renderPassAttachments;
    std::vector<SubpassConfiguration> subpassConfigurations;
    std::vector<vk::SubpassDependency> subpassDependencies;
    std::vector<vk::ClearValue> attachmentClearValues;

    void addAttachment(const vk::AttachmentDescription& attachmentDescription);

    void setAttachments(const vk::ArrayProxy<vk::AttachmentDescription>& attachmentDescriptions);

    void addSubpass(const SubpassConfiguration& subpassConfiguration);

    void setSubpasses(const vk::ArrayProxy<SubpassConfiguration>& subpassConfigurations);

    void addSubpassDependency(const vk::SubpassDependency& subpassDependency);

    void setSubpassDependencies(const vk::ArrayProxy<vk::SubpassDependency>& subpassDependencies);

    void setClearValues(const vk::ArrayProxy<vk::ClearValue>& clearValues);
    void setClearValues(const std::unordered_map<uint32_t, vk::ClearValue>& clearValues);

    void setClearValue(const uint32_t attachment, const vk::ClearValue& clearValue);
    void setClearColour(const uint32_t attachment, const glm::vec4& colour);
    void setClearDepth(const uint32_t attachment, const float& depth);
    void setClearStencil(const uint32_t attachment, const uint32_t& stencil);
};

class RenderPass {
    NO_COPY(RenderPass)
private:
    RenderPass(std::weak_ptr<vkr::Device> device, vk::RenderPass renderPass, const RenderPassConfiguration& config);

public:
    ~RenderPass();

    static RenderPass* create(const RenderPassConfiguration& renderPassConfiguration);

    void begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height, const vk::SubpassContents& subpassContents);
    void begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height, const vk::SubpassContents& subpassContents);
    void begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, const vk::SubpassContents& subpassContents);
    void begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, const vk::SubpassContents& subpassContents);

    const vk::RenderPass& getRenderPass() const;

    const RenderPassConfiguration& getConfiguration() const;

    size_t getAttachmentCount() const;

    size_t getColourAttachmentCount() const;

    void setClearValue(const uint32_t attachment, const vk::ClearValue& clearValue);
    void setClearColour(const uint32_t attachment, const glm::vec4& colour);
    void setClearDepth(const uint32_t attachment, const float& depth);
    void setClearStencil(const uint32_t attachment, const uint32_t& stencil);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::RenderPass m_renderPass;
    RenderPassConfiguration m_config;
    size_t m_colourAttachmentCount;
};


#endif //WORLDENGINE_RENDERPASS_H
