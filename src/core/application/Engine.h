
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

    GraphicsManager* getGraphics() const;

    Scene* getScene() const;

    PhysicsSystem* getPhysicsSystem() const;

    UIRenderer* getUIRenderer() const;

    SceneRenderer* getSceneRenderer() const;

    LightRenderer* getLightRenderer() const;

    ImmediateRenderer* getImmediateRenderer() const;

    DeferredRenderer* getDeferredRenderer() const;

    ReprojectionRenderer* getReprojectionRenderer() const;

    PostProcessRenderer* getPostProcessingRenderer() const;

    EventDispatcher* getEventDispatcher() const;

    const uint64_t& getCurrentFrameCount() const;

    const double& getPartialFrames() const;

    const double& getPartialTicks() const;

    const double& getAccumulatedTime();

    const double& getRunTime();

    const bool& isDebugCompositeEnabled() const;

    void setDebugCompositeEnabled(const bool& debugCompositeEnabled);

    static GraphicsManager* graphics();

    static Scene* scene();

    static PhysicsSystem* physicsSystem();

    static UIRenderer* uiRenderer();

    static SceneRenderer* sceneRenderer();

    static LightRenderer* lightRenderer();

    static ImmediateRenderer* immediateRenderer();

    static ReprojectionRenderer* reprojectionRenderer();

    static DeferredRenderer* deferredRenderer();

    static PostProcessRenderer* postProcessingRenderer();

    static EventDispatcher* eventDispatcher();

    static const uint64_t& currentFrameCount();

    static const double& accumulatedTime();

    static const double& runTime();

    static const bool& debugCompositeEnabled();

    static Engine* instance();

private:
    bool init(SDL_Window* windowHandle);

    void preRender(const double& dt);

    void render(const double& dt);

    void preTick(const double& dt);

    void tick(const double& dt);

    void cleanup();

    static void destroy();

private:
    static Engine* s_instance;

    GraphicsManager* m_graphics;
    Scene* m_scene;
    PhysicsSystem* m_physicsSystem;
    UIRenderer* m_uiRenderer;
    SceneRenderer* m_sceneRenderer;
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
    bool m_debugCompositeEnabled;

    RenderCamera* m_renderCamera;
};


#endif //WORLDENGINE_ENGINE_H
