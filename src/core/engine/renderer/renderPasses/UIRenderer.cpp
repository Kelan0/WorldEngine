
#include "core/engine/renderer/renderPasses/UIRenderer.h"

#include "core/application/Application.h"
#include "core/application/InputHandler.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_vulkan.h"
#include "extern/imgui/implot.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/GraphicsManager.h"
#include "core/engine/ui/PerformanceGraphUI.h"
#include "core/util/Logger.h"

#include <SDL2/SDL.h>
#include "extern/imgui/imgui_impl_sdl.h"

void checkVulkanResult(VkResult result);

UIRenderer::UIRenderer():
    m_createdFontsTexture(false) {
    m_imGuiContext = ImGui::CreateContext();
    m_imPlotContext = ImPlot::CreateContext();
}

UIRenderer::~UIRenderer() {
    LOG_INFO("Destroying UIRenderer");

    LOG_INFO("Shutting down ImGui");
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    ImPlot::DestroyContext(m_imPlotContext);
    ImGui::DestroyContext(m_imGuiContext);
}

bool UIRenderer::init(SDL_Window* windowHandle) {
    LOG_INFO("Initializing UIRenderer");

    ImGui::GetIO(); // Initialize IO
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL2_InitForVulkan(windowHandle)) {
        LOG_ERROR("Failed to initialize ImGui SDL implementation for Vulkan");
        return false;
    }

    vk::AttachmentDescription colourAttachment{};
    colourAttachment.setFormat(Engine::graphics()->getColourFormat());
    colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colourAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // eDontCare ?
    colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    SubpassConfiguration subpassConfiguration{};
    subpassConfiguration.addColourAttachment(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));

    vk::SubpassDependency subpassDependency{};
    subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpassDependency.setDstSubpass(0);
    subpassDependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependency.setSrcAccessMask({});
    subpassDependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = Engine::graphics()->getDevice();
    renderPassConfig.addAttachment(colourAttachment);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.addSubpassDependency(subpassDependency);
    renderPassConfig.setClearColour(0, glm::vec4(0.0F, 0.0F, 0.0F, 1.0F));
    m_uiRenderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig, "UIRenderer-RenderPass"));

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = Engine::graphics()->getInstance();
    initInfo.PhysicalDevice = Engine::graphics()->getPhysicalDevice();
    initInfo.Device = **Engine::graphics()->getDevice();
    initInfo.QueueFamily = Engine::graphics()->getGraphicsQueueFamilyIndex();
    initInfo.Queue = **Engine::graphics()->getQueue(QUEUE_GRAPHICS_MAIN);
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = static_cast<VkDescriptorPool>(Engine::graphics()->descriptorPool()->getDescriptorPool());
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = glm::max((uint32_t)2, (uint32_t)CONCURRENT_FRAMES);
    initInfo.ImageCount = glm::max((uint32_t)2, (uint32_t)CONCURRENT_FRAMES);
    initInfo.CheckVkResultFn = checkVulkanResult;
    ImGui_ImplVulkan_Init(&initInfo, static_cast<VkRenderPass>(m_uiRenderPass->getRenderPass()));

    m_createdFontsTexture = false;

    initUI<PerformanceGraphUI>();

    return true;
}

void UIRenderer::processEvent(const SDL_Event* event) {
    PROFILE_SCOPE("UIRenderer::processEvent")
    ImGui_ImplSDL2_ProcessEvent(event);
}

void UIRenderer::preRender(double dt) {
    PROFILE_SCOPE("UIRenderer::preRender")

    if (!m_createdFontsTexture) {
        m_createdFontsTexture = true;
        LOG_INFO("Creating ImGui GPU font texture");

        const vk::CommandBuffer& uiCommandBuffer = Engine::graphics()->beginOneTimeCommandBuffer();

        ImGui_ImplVulkan_CreateFontsTexture(uiCommandBuffer);

        const vk::Queue& queue = **Engine::graphics()->getQueue(QUEUE_GRAPHICS_TRANSFER_MAIN);
        Engine::graphics()->endOneTimeCommandBuffer(uiCommandBuffer, queue);
        queue.waitIdle();
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    if (Application::instance()->input()->keyPressed(SDL_SCANCODE_F1)) {
        setUIEnabled<PerformanceGraphUI>(!isUIEnabled<PerformanceGraphUI>());
    }
}

void UIRenderer::render(double dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("UIRenderer::render")
    PROFILE_BEGIN_GPU_CMD("UIRenderer::render", commandBuffer);

//    ImGui::ShowDemoWindow();
//    ImPlot::ShowDemoWindow();

    for (auto it = m_uis.begin(); it != m_uis.end(); ++it) {
        bool visible = it->second.second;
        UI* ui = it->second.first;
        ui->update(dt);
        if (visible)
            ui->draw(dt);
    }


    m_uiRenderPass->begin(commandBuffer, Engine::graphics()->getCurrentFramebuffer(), vk::SubpassContents::eInline);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    commandBuffer.endRenderPass();

    PROFILE_END_GPU_CMD("UIRenderer::render", commandBuffer);
}


void checkVulkanResult(VkResult result) {
}