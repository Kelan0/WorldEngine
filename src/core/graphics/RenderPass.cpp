
#include "core/graphics/RenderPass.h"
#include "core/graphics/Image2D.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Framebuffer.h"


void SubpassConfiguration::addColourAttachment(const vk::AttachmentReference& attachmentReference) {
    colourAttachments.emplace_back((uint32_t)attachmentReferences.size());
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::addColourAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout) {
    addColourAttachment(vk::AttachmentReference(attachment, imageLayout));
}

void SubpassConfiguration::addInputAttachment(const vk::AttachmentReference& attachmentReference) {
    inputAttachments.emplace_back((uint32_t)attachmentReferences.size());
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::addInputAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout) {
    addInputAttachment(vk::AttachmentReference(attachment, imageLayout));
}

void SubpassConfiguration::addPreserveAttachment(const vk::AttachmentReference& attachmentReference) {
    preserveAttachments.emplace_back((uint32_t)attachmentReferences.size());
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::addPreserveAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout) {
    addPreserveAttachment(vk::AttachmentReference(attachment, imageLayout));
}

void SubpassConfiguration::setDepthStencilAttachment(const vk::AttachmentReference& attachmentReference) {
    depthStencilAttachment = (uint32_t)attachmentReferences.size();
    attachmentReferences.emplace_back(attachmentReference);
}

void SubpassConfiguration::setDepthStencilAttachment(uint32_t attachment, const vk::ImageLayout& imageLayout) {
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

void RenderPassConfiguration::setClearValue(uint32_t attachment, const vk::ClearValue& clearValue) {
    if (attachmentClearValues.size() < attachment)
        attachmentClearValues.resize(attachment);
    attachmentClearValues[attachment] = clearValue;
}

void RenderPassConfiguration::setClearColour(uint32_t attachment, const glm::vec4& colour) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].color.setFloat32({colour.r, colour.g, colour.b, colour.a});
    } else {
        vk::ClearValue clearValue;
        clearValue.color.setFloat32({colour.r, colour.g, colour.b, colour.a});
        setClearValue(attachment, clearValue);
    }
}

void RenderPassConfiguration::setClearDepth(uint32_t attachment, float depth) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].depthStencil.setDepth(depth);
    } else {
        vk::ClearValue clearValue;
        clearValue.depthStencil.setDepth(depth);
        setClearValue(attachment, clearValue);
    }
}

void RenderPassConfiguration::setClearStencil(uint32_t attachment, uint32_t stencil) {
    if (attachment < attachmentClearValues.size()) {
        attachmentClearValues[attachment].depthStencil.setStencil(stencil);
    } else {
        vk::ClearValue clearValue;
        clearValue.depthStencil.setStencil(stencil);
        setClearValue(attachment, clearValue);
    }
}


RenderPass::RenderPass(const WeakResource<vkr::Device>& device, const vk::RenderPass& renderPass, const RenderPassConfiguration& config, const std::string& name):
        GraphicsResource(ResourceType_RenderPass, device, name),
        m_renderPass(renderPass),
        m_config(config) {
}

RenderPass::~RenderPass() {
    (**m_device).destroyRenderPass(m_renderPass);
}

RenderPass* RenderPass::create(const RenderPassConfiguration& renderPassConfiguration, const std::string& name) {

    const vk::Device& device = **renderPassConfiguration.device.lock(name);

    std::vector<vk::AttachmentReference> allSubpassAttachmentRefs;
    std::vector<vk::SubpassDescription> subpasses;

    size_t attachmentReferenceCount = 0;
    for (const auto& subpassConfig : renderPassConfiguration.subpassConfigurations) {
        attachmentReferenceCount += subpassConfig.colourAttachments.size();
        attachmentReferenceCount += subpassConfig.inputAttachments.size();
        if (subpassConfig.depthStencilAttachment < subpassConfig.attachmentReferences.size())
            attachmentReferenceCount += 1;
    }
    allSubpassAttachmentRefs.reserve(attachmentReferenceCount);

    size_t offset;
    for (const auto& subpassConfig : renderPassConfiguration.subpassConfigurations) {
        vk::SubpassDescription& subpassDescription = subpasses.emplace_back();

        if (!subpassConfig.colourAttachments.empty()) {
            offset = allSubpassAttachmentRefs.size();
            for (const auto& colourAttachmentIndex : subpassConfig.colourAttachments)
                allSubpassAttachmentRefs.emplace_back(subpassConfig.attachmentReferences[colourAttachmentIndex]);
            subpassDescription.setPColorAttachments(&allSubpassAttachmentRefs[offset]);
            subpassDescription.setColorAttachmentCount((uint32_t)subpassConfig.colourAttachments.size());
        }

        if (!subpassConfig.inputAttachments.empty()) {
            offset = allSubpassAttachmentRefs.size();
            for (const auto& inputAttachmentIndex : subpassConfig.inputAttachments)
                allSubpassAttachmentRefs.emplace_back(subpassConfig.attachmentReferences[inputAttachmentIndex]);
            subpassDescription.setPInputAttachments(&allSubpassAttachmentRefs[offset]);
            subpassDescription.setInputAttachmentCount((uint32_t)subpassConfig.inputAttachments.size());
        }

        if (!subpassConfig.preserveAttachments.empty()) {
            subpassDescription.setPPreserveAttachments(&subpassConfig.preserveAttachments[0]);
            subpassDescription.setPreserveAttachmentCount((uint32_t)subpassConfig.preserveAttachments.size());
        }

        if (subpassConfig.depthStencilAttachment < subpassConfig.attachmentReferences.size()) {
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

    vk::RenderPass renderPass = nullptr;
    vk::Result result = device.createRenderPass(&renderPassCreateInfo, nullptr, &renderPass);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create RenderPass: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkRenderPass)renderPass, vk::ObjectType::eRenderPass, name);

    return new RenderPass(renderPassConfiguration.device, renderPass, renderPassConfiguration, name);
}

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const vk::Framebuffer& framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, const vk::SubpassContents& subpassContents) const {
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

void RenderPass::begin(const vk::CommandBuffer& commandBuffer, const Framebuffer* framebuffer, int32_t x, int32_t y, uint32_t width, uint32_t height, const vk::SubpassContents& subpassContents) const {
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

uint32_t RenderPass::getSubpassCount() const {
    return (uint32_t)m_config.subpassConfigurations.size();
}

uint32_t RenderPass::getAttachmentCount() const {
    return (uint32_t)m_config.renderPassAttachments.size();
}

uint32_t RenderPass::getAttachmentCount(uint32_t subpass) const {
    assert(subpass < getSubpassCount());
    const SubpassConfiguration& subpassConfiguration = m_config.subpassConfigurations[subpass];
    return (uint32_t)subpassConfiguration.attachmentReferences.size();
}

uint32_t RenderPass::getColourAttachmentCount(uint32_t subpass) const {
    assert(subpass < getSubpassCount());
    const SubpassConfiguration& subpassConfiguration = m_config.subpassConfigurations[subpass];
    return (uint32_t)subpassConfiguration.colourAttachments.size();
}

bool RenderPass::hasDepthStencilAttachment(uint32_t subpass) const {
    assert(subpass < getSubpassCount());
    const SubpassConfiguration& subpassConfiguration = m_config.subpassConfigurations[subpass];
    return subpassConfiguration.depthStencilAttachment >= 0 && subpassConfiguration.depthStencilAttachment < subpassConfiguration.attachmentReferences.size();
}

void RenderPass::setClearValue(uint32_t attachment, const vk::ClearValue& clearValue) {
    m_config.setClearValue(attachment, clearValue);
}

void RenderPass::setClearColour(uint32_t attachment, const glm::vec4& colour) {
    m_config.setClearColour(attachment, colour);
}

void RenderPass::setClearDepth(uint32_t attachment, float depth) {
    m_config.setClearDepth(attachment, depth);
}

void RenderPass::setClearStencil(uint32_t attachment, uint32_t stencil) {
    m_config.setClearStencil(attachment, stencil);
}


