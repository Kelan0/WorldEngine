
#ifndef WORLDENGINE_ENGINE_H
#define WORLDENGINE_ENGINE_H

#include "core/core.h"

class SDL_Window;
class GraphicsManager;
class Scene;
class SceneRenderer;
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
    GraphicsManager* getGraphics() const;

    Scene* getScene() const;

    SceneRenderer* getSceneRenderer() const;

    ImmediateRenderer* getImmediateRenderer() const;

    DeferredGeometryRenderPass* getDeferredGeometryPass() const;

    DeferredLightingRenderPass* getDeferredLightingPass() const;

    EventDispatcher* getEventDispatcher() const;

    static GraphicsManager* graphics();

    static Scene* scene();

    static SceneRenderer* sceneRenderer();

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
    ImmediateRenderer* m_immediateRenderer;
    DeferredGeometryRenderPass* m_deferredGeometryPass;
    DeferredLightingRenderPass* m_deferredLightingPass;
    EventDispatcher* m_eventDispatcher;
};


#endif //WORLDENGINE_ENGINE_H
