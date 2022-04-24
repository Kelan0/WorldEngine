#include "core/engine/renderer/DeferredRenderer.h"
#include "core/application/Application.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/GraphicsPipeline.h"

DeferredRenderer::DeferredRenderer() {

}

DeferredRenderer::~DeferredRenderer() {

}

void DeferredRenderer::init() {
    createRenderPass();
}

void DeferredRenderer::render(double dt) {

}

void DeferredRenderer::createRenderPass() {
//    RenderPassConfiguration renderPassConfig;
//    renderPassConfig.device = Application::instance()->graphics()->getDevice();
//    vk::AttachmentDescription colourAttachment;
//    colourAttachment.setFormat(getColourFormat());
//    colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
//    colourAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//    colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
//    colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//    colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
//    colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//    colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
//    renderPassConfig.addAttachment(colourAttachment);
//    vk::AttachmentDescription depthAttachment;
//    depthAttachment.setFormat(getDepthFormat());
//    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
//    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
//    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
//    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//    depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
//    renderPassConfig.addAttachment(depthAttachment);
//    SubpassConfiguration subpassConfiguration;
//    subpassConfiguration.addColourAttachment(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));
//    subpassConfiguration.setDepthStencilAttachment(vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal));
//    renderPassConfig.addSubpass(subpassConfiguration);
//    vk::SubpassDependency subpassDependency;
//    subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
//    subpassDependency.setDstSubpass(0);
//    subpassDependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
//    subpassDependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
//    subpassDependency.setSrcAccessMask({});
//    subpassDependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
//    renderPassConfig.addSubpassDependency(subpassDependency);
//
//    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig));
//    return m_renderPass != nullptr;
}