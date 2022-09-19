
#include "core/engine/renderer/renderPasses/UIRenderer.h"

#include "core/imgui/imgui_impl_sdl.h"
#include "core/imgui/imgui_impl_vulkan.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/CommandPool.h"

void checkVulkanResult(VkResult result);

UIRenderer::UIRenderer():
    m_createdFontsTexture(false) {
    m_imGuiContext = ImGui::CreateContext();
}

UIRenderer::~UIRenderer() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext(m_imGuiContext);
}

bool UIRenderer::init(SDL_Window* windowHandle) {

    ImGui::GetIO(); // Initialize IO
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForVulkan(windowHandle)) {
        printf("Failed to initialize ImGui SDL implementation for Vulkan\n");
        return false;
    }

    vk::AttachmentDescription colourAttachment;
    colourAttachment.setFormat(Engine::graphics()->getColourFormat());
    colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colourAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // eDontCare ?
    colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    SubpassConfiguration subpassConfiguration;
    subpassConfiguration.addColourAttachment(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));

    vk::SubpassDependency subpassDependency;
    subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpassDependency.setDstSubpass(0);
    subpassDependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependency.setSrcAccessMask({});
    subpassDependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    RenderPassConfiguration renderPassConfig;
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.addAttachment(colourAttachment);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.addSubpassDependency(subpassDependency);
    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));
    m_uiRenderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig));

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = Engine::graphics()->getInstance();
    initInfo.PhysicalDevice = Engine::graphics()->getPhysicalDevice();
    initInfo.Device = **Engine::graphics()->getDevice();
    initInfo.QueueFamily = Engine::graphics()->getGraphicsQueueFamilyIndex();
    initInfo.Queue = **Engine::graphics()->getQueue(QUEUE_GRAPHICS_MAIN);
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = Engine::graphics()->descriptorPool()->getDescriptorPool();
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.CheckVkResultFn = checkVulkanResult;
    ImGui_ImplVulkan_Init(&initInfo, m_uiRenderPass->getRenderPass());

    m_createdFontsTexture = false;

    return true;
}

void UIRenderer::preRender(double dt) {

    if (!m_createdFontsTexture) {
        m_createdFontsTexture = true;
        printf("Creating ImGui GPU font texture\n");

        const vk::CommandBuffer& uiCommandBuffer = Engine::graphics()->beginOneTimeCommandBuffer();

        ImGui_ImplVulkan_CreateFontsTexture(uiCommandBuffer);

        const vk::Queue& queue = **Engine::graphics()->getQueue(QUEUE_GRAPHICS_TRANSFER_MAIN);
        Engine::graphics()->endOneTimeCommandBuffer(uiCommandBuffer, queue);
        queue.waitIdle();
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void UIRenderer::render(double dt, const vk::CommandBuffer& commandBuffer) {
    m_uiRenderPass->begin(commandBuffer, Engine::graphics()->getCurrentFramebuffer(), vk::SubpassContents::eInline);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    commandBuffer.endRenderPass();
}


void checkVulkanResult(VkResult result) {
}