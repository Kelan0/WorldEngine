#ifndef WORLDENGINE_UIRENDERER_H
#define WORLDENGINE_UIRENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/RenderPass.h"
#include "core/imgui/imgui.h"

class UIRenderer {
public:
    UIRenderer();

    ~UIRenderer();

    bool init(SDL_Window* windowHandle);

    void preRender(double dt);

    void render(double dt, const vk::CommandBuffer& commandBuffer);

private:
    ImGuiContext* m_imGuiContext;
    std::shared_ptr<RenderPass> m_uiRenderPass;
    bool m_createdFontsTexture;
};


#endif //WORLDENGINE_UIRENDERER_H
