
#include "RenderPass.h"


void SubpassConfiguration::addColourAttachment(const vk::AttachmentReference& attachmentReference) {
    colourAttachments.emplace_back(attachmentReferences.size());
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference) {
    depthStencilAttachment = attachmentReferences.size();
    attachmentReferences.emplace_back(attachmentReference);
}


void RenderPassConfiguration::addAttachment(const vk::AttachmentDescription& attachmentDescription) {
    renderPassAttachments.emplace_back(attachmentDescription);
}

void RenderPassConfiguration::addSubpass(const SubpassConfiguration& subpassConfiguration) {
    subpassConfigurations.emplace_back(subpassConfiguration);
}

void RenderPassConfiguration::addSubpassDependency(const vk::SubpassDependency& subpassDependency) {
    subpassDependencies.emplace_back(subpassDependency);
}



RenderPass::RenderPass(std::weak_ptr<vkr::Device> device, vk::RenderPass renderPass, const RenderPassConfiguration& config):
    m_device(device),
    m_renderPass(renderPass),
    m_config(config) {

    m_colourAttachmentCount = 0;
    for (const auto& attachment : config.renderPassAttachments) {
        if (!isDepthAttachment(attachment.format) && !isStencilAttachment(attachment.format))
            ++m_colourAttachmentCount; // We are assuming anything that is not depth/stencil is colour. Is this assumption correct?
    }

}

RenderPass::~RenderPass() {
    (**m_device).destroyRenderPass(m_renderPass);
}

RenderPass* RenderPass::create(const RenderPassConfiguration& renderPassConfiguration) {

    const vk::Device& device = **renderPassConfiguration.device.lock();

    std::vector<vk::AttachmentReference> allSubpassAttachmentRefs;
    std::vector<vk::SubpassDescription> subpasses;

    size_t attachmentReferenceCount = 0;
    for (const auto& subpassConfig : renderPassConfiguration.subpassConfigurations) {
        attachmentReferenceCount += subpassConfig.colourAttachments.size() + 1;
    }
    allSubpassAttachmentRefs.reserve(attachmentReferenceCount);

    size_t offset;
    for (const auto& subpassConfig : renderPassConfiguration.subpassConfigurations) {
        vk::SubpassDescription& subpassDescription = subpasses.emplace_back();

        if (!subpassConfig.colourAttachments.empty()) {
            offset = allSubpassAttachmentRefs.size();
            for (const auto &colourAttachmentIndex: subpassConfig.colourAttachments)
                allSubpassAttachmentRefs.emplace_back(subpassConfig.attachmentReferences[colourAttachmentIndex]);
            subpassDescription.setPColorAttachments(&allSubpassAttachmentRefs[offset]);
            subpassDescription.setColorAttachmentCount(subpassConfig.colourAttachments.size());
        }

        if (subpassConfig.depthStencilAttachment != (size_t)(-1)) {
            offset = allSubpassAttachmentRefs.size();
            allSubpassAttachmentRefs.emplace_back(subpassConfig.attachmentReferences[subpassConfig.depthStencilAttachment]);
            subpassDescription.setPDepthStencilAttachment(&allSubpassAttachmentRefs[offset]);
        }

        // TODO: add other attachment types
    }

    vk::RenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.setAttachments(renderPassConfiguration.renderPassAttachments);
    renderPassCreateInfo.setDependencies(renderPassConfiguration.subpassDependencies);
    renderPassCreateInfo.setSubpasses(subpasses);

    vk::RenderPass renderPass = VK_NULL_HANDLE;
    vk::Result result = device.createRenderPass(&renderPassCreateInfo, NULL, &renderPass);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create RenderPass: %s\n", vk::to_string(result).c_str());
        return NULL;
    }

    return new RenderPass(renderPassConfiguration.device, renderPass, renderPassConfiguration);
}

const vk::RenderPass& RenderPass::getRenderPass() const {
    return m_renderPass;
}

const RenderPassConfiguration& RenderPass::getConfiguration() const {
    return m_config;
}

size_t RenderPass::getAttachmentCount() const {
    return m_config.renderPassAttachments.size();
}

size_t RenderPass::getColourAttachmentCount() const {
    return m_colourAttachmentCount;
}

bool RenderPass::isDepthAttachment(const vk::Format& format) {
    switch (format) {
        case vk::Format::eD16Unorm:
        case vk::Format::eX8D24UnormPack32:
        case vk::Format::eD32Sfloat:
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return true;
        default:
            return false;
    }
}

bool RenderPass::isStencilAttachment(const vk::Format& format) {
    switch (format) {
        case vk::Format::eS8Uint:
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return true;
        default:
            return false;
    }
}
