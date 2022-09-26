#ifndef WORLDENGINE_UIRENDERER_H
#define WORLDENGINE_UIRENDERER_H

#include "core/core.h"
#include "core/engine/ui/UI.h"
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

    template<typename T>
    void setUIEnabled(const bool& enabled);

    template<typename T>
    bool isUIEnabled();

private:
    ImGuiContext* m_imGuiContext;
    std::shared_ptr<RenderPass> m_uiRenderPass;
    bool m_createdFontsTexture;
    std::unordered_map<std::type_index, std::pair<UI*, bool>> m_uis;
};


template<typename T>
void UIRenderer::setUIEnabled(const bool& enabled) {
    static_assert(std::is_base_of<UI, T>::value);

    auto type = std::type_index(typeid(T));
    auto it = m_uis.find(type);
    if (it == m_uis.end()) {
        printf("Creating UI: %s\n", type.name());
        m_uis.insert(std::make_pair(type, std::make_pair(new T(), enabled)));
    } else {
        it->second.second = enabled;
    }

    printf("%s UI %s\n", enabled ? "enabling" : "disabling",  type.name());
}

template<typename T>
bool UIRenderer::isUIEnabled() {
    static_assert(std::is_base_of<UI, T>::value);

    auto type = std::type_index(typeid(T));
    auto it = m_uis.find(type);
    if (it == m_uis.end()) {
        return false;
    } else {
        return it->second.second;
    }
}

#endif //WORLDENGINE_UIRENDERER_H
