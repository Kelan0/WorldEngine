
#ifndef WORLDENGINE_ENGINE_H
#define WORLDENGINE_ENGINE_H

#include "core/core.h"

struct SDL_Window;
class GraphicsManager;
class Scene;
class SceneRenderer;
class LightRenderer;
class ImmediateRenderer;
class DeferredGeometryRenderPass;
class DeferredLightingRenderPass;
class EventDispatcher;
class RenderCamera;

class Engine {
    friend class Application;
private:
    Engine();

    ~Engine();

public:
    [[nodiscard]] GraphicsManager* getGraphics() const;

    [[nodiscard]] Scene* getScene() const;

    [[nodiscard]] SceneRenderer* getSceneRenderer() const;

    [[nodiscard]] LightRenderer* getLightRenderer() const;

    [[nodiscard]] ImmediateRenderer* getImmediateRenderer() const;

    [[nodiscard]] DeferredGeometryRenderPass* getDeferredGeometryPass() const;

    [[nodiscard]] DeferredLightingRenderPass* getDeferredLightingPass() const;

    [[nodiscard]] EventDispatcher* getEventDispatcher() const;

    static GraphicsManager* graphics();

    static Scene* scene();

    static SceneRenderer* sceneRenderer();

    static LightRenderer* lightRenderer();

    static ImmediateRenderer* immediateRenderer();

    static DeferredGeometryRenderPass* deferredGeometryPass();

    static DeferredLightingRenderPass* deferredLightingPass();

    static EventDispatcher* eventDispatcher();

    static Engine* instance();

private:
    bool init(SDL_Window* windowHandle);

    void render(double dt);

    static void destroy();

private:
    static Engine* s_instance;

    GraphicsManager* m_graphics;
    Scene* m_scene;
    SceneRenderer* m_sceneRenderer;
    LightRenderer* m_lightRenderer;
    ImmediateRenderer* m_immediateRenderer;
    DeferredGeometryRenderPass* m_deferredGeometryPass;
    DeferredLightingRenderPass* m_deferredLightingPass;
    EventDispatcher* m_eventDispatcher;

    RenderCamera* m_renderCamera;
};


#endif //WORLDENGINE_ENGINE_H
