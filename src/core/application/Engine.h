
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
class ReprojectionRenderer;
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

    [[nodiscard]] DeferredRenderer* getDeferredRenderer() const;

    [[nodiscard]] ReprojectionRenderer* getReprojectionRenderer() const;

    [[nodiscard]] PostProcessRenderer* getPostProcessingRenderer() const;

    [[nodiscard]] EventDispatcher* getEventDispatcher() const;

    [[nodiscard]] const uint64_t& getCurrentFrameCount() const;

    [[nodiscard]] const bool& isDebugCompositeEnabled() const;

    void setDebugCompositeEnabled(const bool& debugCompositeEnabled);

    static GraphicsManager* graphics();

    static Scene* scene();

    static UIRenderer* uiRenderer();

    static SceneRenderer* sceneRenderer();

    static LightRenderer* lightRenderer();

    static ImmediateRenderer* immediateRenderer();

    static ReprojectionRenderer* reprojectionRenderer();

    static DeferredRenderer* deferredRenderer();

    static PostProcessRenderer* postProcessingRenderer();

    static EventDispatcher* eventDispatcher();

    static const uint64_t& currentFrameCount();

    static const bool& debugCompositeEnabled();

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
    ReprojectionRenderer* m_reprojectionRenderer;
    PostProcessRenderer* m_postProcessingRenderer;
    EventDispatcher* m_eventDispatcher;
    uint64_t m_currentFrameCount;
    bool m_debugCompositeEnabled;

    RenderCamera* m_renderCamera;
};


#endif //WORLDENGINE_ENGINE_H
