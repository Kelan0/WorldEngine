
#ifndef WORLDENGINE_ENGINE_H
#define WORLDENGINE_ENGINE_H

#include "core/core.h"

struct SDL_Window;
union SDL_Event;
class GraphicsManager;
class Scene;
class UIRenderer;
class SceneRenderer;
class LightRenderer;
class ImmediateRenderer;
class DeferredGeometryRenderPass;
class DeferredRenderer;
class PostProcessRenderer;
class EventDispatcher;
class RenderCamera;

class Engine {
    friend class Application;
private:
    Engine();

    ~Engine();

public:
    void processEvent(const SDL_Event* event);

    [[nodiscard]] GraphicsManager* getGraphics() const;

    [[nodiscard]] Scene* getScene() const;

    [[nodiscard]] UIRenderer* getUIRenderer() const;

    [[nodiscard]] SceneRenderer* getSceneRenderer() const;

    [[nodiscard]] LightRenderer* getLightRenderer() const;

    [[nodiscard]] ImmediateRenderer* getImmediateRenderer() const;

    [[nodiscard]] DeferredRenderer* getDeferredLightingPass() const;

    [[nodiscard]] PostProcessRenderer* getPostProcessingRenderer() const;

    [[nodiscard]] EventDispatcher* getEventDispatcher() const;

    static GraphicsManager* graphics();

    static Scene* scene();

    static UIRenderer* uiRenderer();

    static SceneRenderer* sceneRenderer();

    static LightRenderer* lightRenderer();

    static ImmediateRenderer* immediateRenderer();

    static DeferredRenderer* deferredLightingPass();

    static PostProcessRenderer* postProcessingRenderer();

    static EventDispatcher* eventDispatcher();

    static Engine* instance();

private:
    bool init(SDL_Window* windowHandle);

    void preRender(const double& dt);

    void render(const double& dt);

    static void destroy();

private:
    static Engine* s_instance;

    GraphicsManager* m_graphics;
    Scene* m_scene;
    UIRenderer* m_uiRenderer;
    SceneRenderer* m_sceneRenderer;
    LightRenderer* m_lightRenderer;
    ImmediateRenderer* m_immediateRenderer;
    DeferredRenderer* m_deferredRenderer;
    PostProcessRenderer* m_postProcessingRenderer;
    EventDispatcher* m_eventDispatcher;

    RenderCamera* m_renderCamera;
};


#endif //WORLDENGINE_ENGINE_H
