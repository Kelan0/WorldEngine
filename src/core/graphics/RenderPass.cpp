
#include "core/graphics/RenderPass.h"
#include "core/graphics/Image2D.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Framebuffer.h"


void SubpassConfiguration::addColourAttachment(const vk::AttachmentReference& attachmentReference) {
    colourAttachments.emplace_back(attachmentReferences.size());
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::addColourAttachment(const uint32_t& attachment, const vk::ImageLayout& imageLayout) {
    addColourAttachment(vk::AttachmentReference(attachment, imageLayout));
}

void SubpassConfiguration::setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference) {
    depthStencilAttachment = attachmentReferences.size();
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::setDepthStencilAttachment(const uint32_t& attachment, const vk::ImageLayout& imageLayout) {
    setDepthStencilAttachment(vk::AttachmentReference(attachment, imageLayout));
}


void RenderPassConfiguration::addAttachment(const vk::AttachmentDescription& attachmentDescription) {
    renderPassAttachments.emplace_back(attachmentDescription);
    if (attachmentClearValues.size() < renderPassAttachments.size())
        attachmentClearValues.resize(renderPassAttachments.size());
}

void RenderPassConfiguration::setAttachments(const vk::ArrayProxy<vk::AttachmentDescription>& attachmentDescriptions) {
    renderPassAttachments.clear();
    for (const auto& attachment : attachmentDescriptions)
        addAttachment(attachment);
}

void RenderPassConfiguration::addSubpass(const SubpassConfiguration& subpassConfiguration) {
    subpassConfigurations.emplace_back(subpassConfiguration);
}

void RenderPassConfiguration::setSubpasses(const vk::ArrayProxy<SubpassConfiguration>& subpassConfigurations) {
    this->subpassConfigurations.clear();
    for (const auto& subpassConfiguration : subpassConfigurations)
        addSubpass(subpassConfiguration);
}

void RenderPassConfiguration::addSubpassDependency(const vk::SubpassDependency& subpassDependency) {
    subpassDependencies.emplace_back(subpassDependency);
}

void RenderPassConfiguration::setSubpassDependencies(const vk::ArrayProxy<vk::SubpassDependency>& subpassDependencies) {
    this->subpassDependencies.clear();
    for (const auto& dependency : subpassDependencies)
        addSubpassDependency(dependency);
}

void RenderPassConfiguration::setClearValues(const vk::ArrayProxy<vk::ClearValue>& clearValues) {
    attachmentClearValues.clear();
    for (const auto& clearValue : clearValues)
        attachmentClearValues.emplace_back(clearValue);
    if (attachmentClearValues.size() < renderPassAttachments.size())
        attachmentClearValues.resize(renderPassAttachments.size());
}
void RenderPassConfiguration::setClearValues(const std::unordered_map<uint32_t, vk::ClearValue>& clearValues) {
    for (auto it = clearValues.begin(); it != clearValues.end(); ++it)
        setClearValue(it->first, it->second);
}

void RenderPassConfiguration::setClearValue(const uint32_t attachment, const vk::ClearValue& clearValue) {
    if (attachmentClearValues.size() < attachment)
        attachmentClearValues.resize(attachment);
    attachmentClearValues[attachment] = clearValue;
}

void RenderPassConfiguration::setClearColour(const uint32_t attachment, const glm::vec4& colour) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].color.setFloat32({ colour.r, colour.g, colour.b, colour.a });
    } else {
        vk::ClearValue clearValue;
        clearValue.color.setFloat32({ colour.r, colour.g, colour.b, colour.a });
        setClearValue(attachment, clearValue);
    }
}

void RenderPassConfiguration::setClearDepth(const uint32_t attachment, const float& depth) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].depthStencil.setDepth(depth);
    } else {
        vk::ClearValue clearValue;
        clearValue.depthStencil.setDepth(depth);
        setClearValue(attachment, clearValue);
    }
}

void RenderPassConfiguration::setClearStencil(const uint32_t attachment, const uint32_t& stencil) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].depthStencil.setStencil(stencil);
    } else {
        vk::ClearValue clearValue;
        clearValue.depthStencil.setStencil(stencil);
        setClearValue(attachment, clearValue);
    }
}




RenderPass::RenderPass(std::weak_ptr<vkr::Device> device, vk::RenderPass renderPass, const RenderPassConfiguration& config):
    m_device(device),
    m_renderPass(renderPass),
    m_config(config) {

    // TODO: we should use the SubpassConfigurations to determine which attachments are colour attachments.
    // The number of colour attachments isn't really specific to a RenderPass, but is to a Subpass
    m_colourAttachmentCount = 0;
    for (const auto& attachment : config.renderPassAttachments) {
        if (!ImageUtil::isDepthAttachment(attachment.format) && !ImageUtil::isStencilAttachment(attachment.format))
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
            subpassDescription.setColorAttachmentCount((uint32_t)subpassConfig.colourAttachments.size());
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

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height, const vk::SubpassContents& subpassContents) const {
    vk::RenderPassBeginInfo beginInfo;
    beginInfo.setRenderPass(m_renderPass);
    beginInfo.setFramebuffer(framebuffer);
    beginInfo.renderArea.offset.x = x;
    beginInfo.renderArea.offset.y = y;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.setClearValues(m_config.attachmentClearValues);
    commandBuffer.beginRenderPass(beginInfo, subpassContents);
}

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height, const vk::SubpassContents& subpassContents) const {
    begin(commandBuffer, framebuffer->getFramebuffer(), x, y, width, height, subpassContents);
}

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, const vk::SubpassContents& subpassContents) const {
    const auto& extent = Engine::graphics()->getImageExtent();
    begin(commandBuffer, framebuffer, 0, 0, extent.width, extent.height, subpassContents);
}

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, const vk::SubpassContents& subpassContents) const {
    begin(commandBuffer, framebuffer->getFramebuffer(), 0, 0, framebuffer->getWidth(), framebuffer->getHeight(), subpassContents);
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

void RenderPass::setClearValue(const uint32_t attachment, const vk::ClearValue& clearValue) {
    m_config.setClearValue(attachment, clearValue);
}

void RenderPass::setClearColour(const uint32_t attachment, const glm::vec4& colour) {
    m_config.setClearColour(attachment, colour);
}

void RenderPass::setClearDepth(const uint32_t attachment, const float& depth) {
    m_config.setClearDepth(attachment, depth);
}

void RenderPass::setClearStencil(const uint32_t attachment, const uint32_t& stencil) {
    m_config.setClearStencil(attachment, stencil);
}


