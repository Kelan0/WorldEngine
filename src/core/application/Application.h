
#ifndef WORLDENGINE_APPLICATION_H
#define WORLDENGINE_APPLICATION_H

#include "core/core.h"
#include <SDL.h>

class GraphicsManager;
class InputHandler;
class Scene;
class SceneRenderer;
class ImmediateRenderer;
class DeferredRenderer;
class EventDispatcher;

class Application {
    NO_COPY(Application);

public:
    Application();

    ~Application();

    virtual void init() = 0;

    virtual void cleanup() = 0;

    virtual void render(double dt) = 0;

    template<class T>
    static int create();

    static void destroy();

    static Application* instance();

    void stop();

    GraphicsManager* graphics();

    InputHandler* input();

    Scene* scene();

    SceneRenderer* sceneRenderer();

    ImmediateRenderer* immediateRenderer();

    DeferredRenderer* deferredRenderer();

    EventDispatcher* eventDispatcher();

    glm::ivec2 getWindowSize() const;

    double getWindowAspectRatio() const;

    bool isViewportInverted() const;

private:
    void start();

    bool initInternal();

    void cleanupInternal();

    void renderInternal(double dt);

    void processEventsInternal();

private:
    static Application* s_instance;

    SDL_Window* m_windowHandle;
    GraphicsManager* m_graphics;
    InputHandler* m_inputHandler;
    Scene* m_scene;
    SceneRenderer* m_sceneRenderer;
    ImmediateRenderer* m_immediateRenderer;
    DeferredRenderer* m_deferredRenderer;
    EventDispatcher* m_eventDispatcher;

    bool m_running;
};

template<class T>
inline int Application::create() {
    printf("Creating application\n");

    constexpr bool isApplication = std::is_base_of<Application, T>::value;
    if (!isApplication) {
        printf("Engine must be created with an instance of the Application class\n");
        assert(false);
        return -1;
    }

    s_instance = new T();
    if (!s_instance->initInternal()) {
        Application::destroy();
        return -1;
    }

    s_instance->start();
    Application::destroy();
    return 0;
}

#endif //WORLDENGINE_APPLICATION_H
