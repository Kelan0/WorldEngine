
#ifndef WORLDENGINE_ENGINE_H
#define WORLDENGINE_ENGINE_H

#include "core/core.h"

struct SDL_Window;
union SDL_Event;
class GraphicsManager;
class Scene;
class PhysicsSystem;
class UIRenderer;
class SceneRenderer;
class TerrainRenderer;
class LightRenderer;
class ImmediateRenderer;
class ReprojectionRenderer;
class DeferredRenderer;
class PostProcessRenderer;
class EventDispatcher;
class RenderCamera;
class Frustum;

class Engine {
    friend class Application;
private:
    Engine();

    ~Engine();

public:
    void processEvent(const SDL_Event* event);

    GraphicsManager* getGraphics() const;

    Scene* getScene() const;

    PhysicsSystem* getPhysicsSystem() const;

    UIRenderer* getUIRenderer() const;

    SceneRenderer* getSceneRenderer() const;

    TerrainRenderer* getTerrainRenderer() const;

    LightRenderer* getLightRenderer() const;

    ImmediateRenderer* getImmediateRenderer() const;

    DeferredRenderer* getDeferredRenderer() const;

    ReprojectionRenderer* getReprojectionRenderer() const;

    PostProcessRenderer* getPostProcessingRenderer() const;

    EventDispatcher* getEventDispatcher() const;

    uint64_t getCurrentFrameCount() const;

    uint32_t getSwapchainFrameIndex() const;

    double getPartialFrames() const;

    double getPartialTicks() const;

    double getAccumulatedTime();

    double getRunTime();

    bool isDebugCompositeEnabled() const;

    bool isViewFrustumPaused() const;

    void setViewFrustumPaused(bool viewFrustumPaused);

    bool isRenderWireframeEnabled() const;

    void setRenderWireframeEnabled(bool renderWireframeEnabled);

    void setDebugCompositeEnabled(bool debugCompositeEnabled);

    const RenderCamera* getRenderCamera() const;

    const Frustum* getViewFrustum() const;

    static GraphicsManager* graphics();

    static Scene* scene();

    static EventDispatcher* eventDispatcher();

    static Engine* instance();

private:
    bool init(SDL_Window* windowHandle);

    void preRender(double dt);

    void render(double dt);

    void preTick(double dt);

    void tick(double dt);

    void cleanup();

    static void destroy();

private:
    static Engine* s_instance;

    GraphicsManager* m_graphics;
    Scene* m_scene;
    PhysicsSystem* m_physicsSystem;
    UIRenderer* m_uiRenderer;
    SceneRenderer* m_sceneRenderer;
    TerrainRenderer* m_terrainRenderer;
    LightRenderer* m_lightRenderer;
    ImmediateRenderer* m_immediateRenderer;
    DeferredRenderer* m_deferredRenderer;
    ReprojectionRenderer* m_reprojectionRenderer;
    PostProcessRenderer* m_postProcessingRenderer;
    EventDispatcher* m_eventDispatcher;
    uint64_t m_currentFrameCount;
    std::chrono::high_resolution_clock::time_point m_startTime;
    double m_accumulatedTime;
    double m_runTime;
    bool m_viewFrustumPaused;
    bool m_renderWireframeEnabled;
    bool m_debugCompositeEnabled;

    RenderCamera* m_renderCamera;
    Frustum* m_viewFrustum;
};


#endif //WORLDENGINE_ENGINE_H
