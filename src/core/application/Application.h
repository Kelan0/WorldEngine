
#ifndef WORLDENGINE_APPLICATION_H
#define WORLDENGINE_APPLICATION_H

#include "core/core.h"
#include "Engine.h"

#include <SDL2/SDL.h>


class InputHandler;

class Application {
    NO_COPY(Application);

public:
    Application();

    virtual ~Application();

    virtual void init() = 0;

    virtual void cleanup() = 0;

    virtual void render(const double& dt) = 0;

    template<class T>
    static int create(int argc, char* argv[]);

    static void destroy();

    static Application* instance();

    void stop();

    InputHandler* input();

    glm::ivec2 getWindowSize() const;

    double getWindowAspectRatio() const;

    const double& getFramerateLimit() const;

    void setFramerateLimit(const double& framerateLimit);

    bool isViewportInverted() const;

    const std::string& getExecutionDirectory() const;

    const std::thread::id& getMainThreadId() const;
    uint64_t getHashedMainThreadId() const;

private:
    void start();

    bool initInternal();

    void cleanupInternal();

    void renderInternal(const double& dt);

    void processEventsInternal();

private:
    static Application* s_instance;

    SDL_Window* m_windowHandle;
    InputHandler* m_inputHandler;
    std::string m_executionDirectory;

    double m_framerateLimit;

    std::thread::id m_mainThreadId;

    bool m_rendering;
    bool m_running;
};

template<class T>
inline int Application::create(int argc, char* argv[]) {
    printf("Creating application\n");

    // TODO: store argc/argv   Add --resdir to specify the resources directory, --glslcomp to specify glslc.exe location

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
