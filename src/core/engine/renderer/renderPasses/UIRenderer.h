#ifndef WORLDENGINE_UIRENDERER_H
#define WORLDENGINE_UIRENDERER_H

#include "core/core.h"
#include "core/util/Util.h"
#include "core/engine/ui/UI.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/RenderPass.h"

struct ImGuiContext;
struct ImPlotContext;
struct SDL_Window;
union SDL_Event;

class UIRenderer {
public:
    UIRenderer();

    ~UIRenderer();

    bool init(SDL_Window* windowHandle);

    void processEvent(const SDL_Event* event);

    void preRender(double dt);

    void render(double dt, const vk::CommandBuffer& commandBuffer);

    template<typename T>
    bool initUI();

    template<typename T>
    void setUIEnabled(bool enabled);

    template<typename T>
    bool isUIEnabled();

private:
    ImGuiContext* m_imGuiContext;
    ImPlotContext* m_imPlotContext;
    std::shared_ptr<RenderPass> m_uiRenderPass;
    bool m_createdFontsTexture;
    std::unordered_map<std::type_index, std::pair<UI*, bool>> m_uis;
};


template<typename T>
bool UIRenderer::initUI() {
    static_assert(std::is_base_of<UI, T>::value);

    auto type = std::type_index(typeid(T));
    auto it = m_uis.find(type);
    if (it == m_uis.end()) {
        m_uis.insert(std::make_pair(type, std::make_pair(new T(), false)));
        return true;
    }
    return false;
}

template<typename T>
void UIRenderer::setUIEnabled(bool enabled) {
    static_assert(std::is_base_of<UI, T>::value);

    auto type = std::type_index(typeid(T));
    auto it = m_uis.find(type);
    assert(it != m_uis.end());
    it->second.second = enabled;
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
