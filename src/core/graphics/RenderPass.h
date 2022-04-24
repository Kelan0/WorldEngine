#ifndef WORLDENGINE_RENDERPASS_H
#define WORLDENGINE_RENDERPASS_H

#include "core/core.h"

struct SubpassConfiguration {
    std::vector<vk::AttachmentReference> attachmentReferences;
    std::vector<size_t> colourAttachments;
    size_t depthStencilAttachment = (size_t)(-1);
    // TODO: resolve attachments, preserve attachments, input attachments

    void addColourAttachment(const vk::AttachmentReference& attachmentReference);

    void setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference);
};

struct RenderPassConfiguration {
    std::weak_ptr<vkr::Device> device;
    std::vector<vk::AttachmentDescription> renderPassAttachments;
    std::vector<SubpassConfiguration> subpassConfigurations;
    std::vector<vk::SubpassDependency> subpassDependencies;

    void addAttachment(const vk::AttachmentDescription& attachmentDescription);

    void addSubpass(const SubpassConfiguration& subpassConfiguration);

    void addSubpassDependency(const vk::SubpassDependency& subpassDependency);
};

class RenderPass {
    NO_COPY(RenderPass)
private:
    RenderPass(std::weak_ptr<vkr::Device> device, vk::RenderPass renderPass, const RenderPassConfiguration& config);

public:
    ~RenderPass();

    static RenderPass* create(const RenderPassConfiguration& renderPassConfiguration);

    const vk::RenderPass& getRenderPass() const;

    const RenderPassConfiguration& getConfiguration() const;

    size_t getAttachmentCount() const;

    size_t getColourAttachmentCount() const;

    static bool isDepthAttachment(const vk::Format& format);

    static bool isStencilAttachment(const vk::Format& format);
private:
    std::shared_ptr<vkr::Device> m_device;
    vk::RenderPass m_renderPass;
    RenderPassConfiguration m_config;
    size_t m_colourAttachmentCount;
};


#endif //WORLDENGINE_RENDERPASS_H
