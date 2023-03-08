#ifndef WORLDENGINE_RENDERPASS_H
#define WORLDENGINE_RENDERPASS_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

class Framebuffer;

struct SubpassConfiguration {
    std::vector<vk::AttachmentReference> attachmentReferences; // List of references to RenderPassConfiguration::renderPassAttachments
    std::vector<uint32_t> colourAttachments; // List of indices into attachmentReferences which represent colour attachments
    std::vector<uint32_t> inputAttachments; // List of indices into attachmentReferences which represent input attachments
    std::vector<uint32_t> preserveAttachments; // List of indices into attachmentReferences which represent preserve attachments
    uint32_t depthStencilAttachment = UINT32_MAX; // Index in attachmentReferences which represents depth/stencil attachment
    // TODO: resolve attachments

    void addColourAttachment(const vk::AttachmentReference& attachmentReference);

    void addColourAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout);

    void addInputAttachment(const vk::AttachmentReference& attachmentReference);

    void addInputAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout);

    void addPreserveAttachment(const vk::AttachmentReference& attachmentReference);

    void addPreserveAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout);

    void setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference);

    void setDepthStencilAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout);
};

struct RenderPassConfiguration {
    WeakResource<vkr::Device> device;
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

    void setClearValue(uint32_t attachment, const vk::ClearValue& clearValue);

    void setClearColour(uint32_t attachment, const glm::vec4& colour);

    void setClearDepth(uint32_t attachment, float depth);

    void setClearStencil(uint32_t attachment, uint32_t stencil);
};

class RenderPass : public GraphicsResource {
    NO_COPY(RenderPass)

private:
    RenderPass(const WeakResource<vkr::Device>& device, const vk::RenderPass& renderPass, const RenderPassConfiguration& config, const std::string& name);

public:
    ~RenderPass() override;

    static RenderPass* create(const RenderPassConfiguration& renderPassConfiguration, const std::string& name);

    void begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, const vk::SubpassContents& subpassContents) const;

    void begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, const vk::SubpassContents& subpassContents) const;

    void begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, const vk::SubpassContents& subpassContents) const;

    void begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, const vk::SubpassContents& subpassContents) const;

    const vk::RenderPass& getRenderPass() const;

    const RenderPassConfiguration& getConfiguration() const;

    uint32_t getSubpassCount() const;

    uint32_t getAttachmentCount() const;

    uint32_t getAttachmentCount(uint32_t subpass) const;

    uint32_t getColourAttachmentCount(uint32_t subpass) const;

    bool hasDepthStencilAttachment(uint32_t subpass) const;

    void setClearValue(uint32_t attachment, const vk::ClearValue& clearValue);

    void setClearColour(uint32_t attachment, const glm::vec4& colour);

    void setClearDepth(uint32_t attachment, float depth);

    void setClearStencil(uint32_t attachment, uint32_t stencil);

private:
    vk::RenderPass m_renderPass;
    RenderPassConfiguration m_config;
};


#endif //WORLDENGINE_RENDERPASS_H
