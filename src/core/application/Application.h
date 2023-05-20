
#ifndef WORLDENGINE_APPLICATION_H
#define WORLDENGINE_APPLICATION_H

#include "core/core.h"
#include "Engine.h"
#include "core/util/Logger.h"

#include <SDL2/SDL.h>

class InputHandler;

class Application {
    NO_COPY(Application);

public:
    Application();

    virtual ~Application();

    virtual void init() = 0;

    virtual void cleanup() = 0;

    virtual void render(double dt) = 0;

    virtual void tick(double dt) = 0;

    template<class T>
    static int create(int argc, char* argv[]);

    static void destroy();

    static Application* instance();

    void stop();

    Logger* logger();

    InputHandler* input();

    glm::ivec2 getWindowSize() const;

    double getWindowAspectRatio() const;

    double getFramerateLimit() const;

    void setFramerateLimit(double framerateLimit);

    double getTickrate() const;

    void setTickrate(double tickrate);

    double getPartialFrames() const;

    double getPartialTicks() const;

    bool isViewportInverted() const;

    const std::string& getExecutionDirectory() const;

    const std::string& getResourceDirectory() const;

    const std::string& getShaderCompilerDirectory() const;

    std::string getAbsoluteResourceFilePath(const std::string& resourceFilePath) const;

    const std::thread::id& getMainThreadId() const;
    uint64_t getHashedMainThreadId() const;

    bool isWindowFocused() const;

    bool isRendering() const;

private:
    void start();

    void shutdownNow();

    void runUpdateThread();

    bool parseArgs(int argc, char* argv[]);

    bool matchesArgWithValue(int argc, char* argv[], int index, const std::vector<const char*>& argNames);

    bool getArgValue(int argc, char* argv[], int& index, const std::vector<const char*>& argNames, char*& outValue);

    bool initInternal();

    void cleanupInternal();

    void renderInternal(double dt);

    void tickInternal(double dt);

    void processEventsInternal();

private:
    static Application* s_instance;

    Logger* m_logger;
    SDL_Window* m_windowHandle;
    InputHandler* m_inputHandler;
    std::string m_executionDirectory;
    std::string m_resourceDirectory;
    std::string m_shaderCompilerDirectory;

    double m_framerateLimit;
    double m_tickrate;

    double m_partialFrames;
    double m_partialTicks;

    std::thread m_updateThread;

    std::thread::id m_mainThreadId;
    std::thread::id m_updateThreadId;

    bool m_focused;
    bool m_rendering;
    bool m_running;
    bool m_shutdown;
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

    if (!s_instance->parseArgs(argc, argv)) {
        Application::destroy();
        return -1;
    }

    if (!s_instance->initInternal()) {
        Application::destroy();
        return -1;
    }

    s_instance->start();
    Application::destroy();
    return 0;
}

#endif //WORLDENGINE_APPLICATION_H
