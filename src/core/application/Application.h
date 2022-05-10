
#ifndef WORLDENGINE_APPLICATION_H
#define WORLDENGINE_APPLICATION_H

#include "core/core.h"
#include <SDL.h>


class InputHandler;

class Application {
    NO_COPY(Application);

public:
    Application();

    virtual ~Application();

    virtual void init() = 0;

    virtual void cleanup() = 0;

    virtual void render(double dt) = 0;

    template<class T>
    static int create(int argc, char* argv[]);

    static void destroy();

    static Application* instance();

    void stop();

    InputHandler* input();

    glm::ivec2 getWindowSize() const;

    double getWindowAspectRatio() const;

    bool isViewportInverted() const;

    const std::string& getExecutionDirectory() const;
private:
    void start();

    bool initInternal();

    void cleanupInternal();

    void renderInternal(double dt);

    void processEventsInternal();

    static std::string findExecutionDirectory();

private:
    static Application* s_instance;

    SDL_Window* m_windowHandle;
    InputHandler* m_inputHandler;
    std::string m_executionDirectory;

    bool m_running;
};

template<class T>
inline int Application::create(int argc, char* argv[]) {
    printf("Creating application\n");

    constexpr bool isApplication = std::is_base_of<Application, T>::value;
    if (!isApplication) {
        printf("Engine must be created with an instance of the Application class\n");
        assert(false);
        return -1;
    }

    s_instance = new T();
    s_instance->m_executionDirectory = Application::findExecutionDirectory();
    if (!s_instance->initInternal()) {
        Application::destroy();
        return -1;
    }

    s_instance->start();
    Application::destroy();
    return 0;
}

#endif //WORLDENGINE_APPLICATION_H
