#include "core/application/Application.h"
#include "core/application/Engine.h"
#include "core/application/InputHandler.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/ApplicationEvents.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/PlatformUtils.h"
#include "core/util/Profiler.h"
#include <chrono>

Application* Application::s_instance = nullptr;


Application::Application():
        m_framerateLimit(0.0), // Unlimited
        m_windowHandle(nullptr),
        m_inputHandler(nullptr),
        m_running(false),
        m_rendering(false) {
}

Application::~Application() {
    delete m_inputHandler;

    printf("Destroying window\n");
    SDL_DestroyWindow(m_windowHandle);

    printf("Quitting SDL\n");
    SDL_Quit();

    s_instance = nullptr;
    printf("Uninitialized application\n");
}

bool Application::initInternal() {
    PROFILE_SCOPE("Application::initInternal")

    m_mainThreadId = std::this_thread::get_id();
    printf("Initializing application on main thread 0x%016llx\n", ThreadUtils::getThreadHashedId(m_mainThreadId));
    m_executionDirectory = PlatformUtils::findExecutionDirectory();

    PROFILE_REGION("Init SDL")

    printf("Initializing SDL\n");
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }


    printf("Creating window\n");
    uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    m_windowHandle = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
    if (m_windowHandle == nullptr) {
        printf("Failed to create SDL window: %s\n", SDL_GetError());
        return false;
    }


    PROFILE_REGION("Init InputHandler")
    m_inputHandler = new InputHandler(m_windowHandle);

    if (!Engine::instance()->init(m_windowHandle)) {
        printf("Failed to initialize engine, unable to continue\n");
        return false;
    }

    PROFILE_REGION("Init Application")
    init();

    return true;
}

void Application::cleanupInternal() {
    cleanup();
}

void Application::renderInternal(const double& dt) {
    PROFILE_SCOPE("Application::renderInternal");

    Engine::instance()->preRender(dt);

    PROFILE_REGION("Application::render - Implementation")
    render(dt);
    PROFILE_END_REGION()

    Engine::instance()->render(dt);

}

void Application::processEventsInternal() {
    PROFILE_SCOPE("Application::processEventsInternal")
    m_inputHandler->update();

    glm::ivec2 windowSize = getWindowSize();

    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
            case SDL_QUIT:
                stop();
                break;

            case SDL_WINDOWEVENT:
                switch (sdlEvent.window.event) {
                    case SDL_WINDOWEVENT_SHOWN: {
                        m_rendering = true;
                        ScreenShowEvent event{getWindowSize()};
                        Engine::eventDispatcher()->trigger(&event);
                        break;
                    }
                    case SDL_WINDOWEVENT_HIDDEN: {
                        m_rendering = false;
                        ScreenHiddenEvent event{};
                        Engine::eventDispatcher()->trigger(&event);
                        break;
                    }
                    case SDL_WINDOWEVENT_MINIMIZED: {
                        m_rendering = false;
                        ScreenMinimisedEvent event{};
                        Engine::eventDispatcher()->trigger(&event);
                        break;
                    }
                    case SDL_WINDOWEVENT_MAXIMIZED: {
                        m_rendering = true;
                        ScreenMaximisedEvent event{};
                        Engine::eventDispatcher()->trigger(&event);
                        break;
                    }
                    case SDL_WINDOWEVENT_RESTORED: {
                        break;
                    }
                    case SDL_WINDOWEVENT_SIZE_CHANGED: {
                        ScreenResizeEvent event{windowSize, getWindowSize()};
                        Engine::eventDispatcher()->trigger(&event);
                        break;
                    }
                }
                break;
        }

        Engine::instance()->processEvent(&sdlEvent);
        m_inputHandler->processEvent(&sdlEvent);
    }

    if (Engine::graphics()->didResolutionChange()) {
        printf("Resolution changed\n");
    }
}

void Application::destroy() {
    assert(s_instance != nullptr);
    Engine::destroy();
    delete Application::s_instance;
    Application::s_instance = nullptr;
}

void Application::start() {
    m_running = true;

    // Trigger a ScreenResizeEvent at the beginning of the render loop so that anything that needs it can be initialized easily
    ScreenResizeEvent event{getWindowSize(), getWindowSize() };
    Engine::eventDispatcher()->trigger(&event);

    std::vector<double> frameTimes;
    std::vector<double> cpuFrameTimes;

    auto lastDebug = std::chrono::high_resolution_clock::now();
    auto lastFrame = std::chrono::high_resolution_clock::now();
    auto lastTime = std::chrono::high_resolution_clock::now();

    double partialFrames = 0.0;

    DebugUtils::RenderInfo tempDebugInfo;

    static profile_id profileID_CPU_Idle = Profiler::id("CPU Idle");

    Profiler::beginFrame();
    Profiler::beginCPU(profileID_CPU_Idle);

    while (m_running) {
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();
        lastTime = now;

        bool isFrame = false;
        double frameDurationNanos = m_framerateLimit < 1.0 ? 1.0 : (1e+9 / m_framerateLimit);

        partialFrames += (double)elapsedNanos / frameDurationNanos;

        Engine::eventDispatcher()->update();

        if (partialFrames >= 1.0) {
            Profiler::endCPU(); // profileID_CPU_Idle
            Profiler::endFrame();
            Profiler::beginFrame();
            isFrame = true;

            partialFrames = 0.0;

            auto beginFrame = now;

            ThreadUtils::wakeThreads();

            processEventsInternal();

            // TODO: update tick independent of render and framerate, possibly on separate thread?

            if (m_rendering) {
                if (Engine::graphics()->beginFrame()) {
                    uint64_t frameElapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastFrame).count();
                    double dt = frameElapsedNanos / 1e+9;

                    auto cpuBegin = std::chrono::high_resolution_clock::now();
                    renderInternal(dt);
                    auto cpuEnd = std::chrono::high_resolution_clock::now();

                    Engine::graphics()->endFrame();

                    tempDebugInfo += Engine::graphics()->debugInfo();

                    lastFrame = now;

                    auto endFrame = std::chrono::high_resolution_clock::now();
                    frameTimes.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(endFrame - beginFrame).count() / 1000000.0);
                    cpuFrameTimes.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(cpuEnd - cpuBegin).count() / 1000000.0);
                }
            }
        }


//        uint64_t debugDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDebug).count();
//
//        if (debugDuration >= 1000000000u) {
//            PROFILE_SCOPE("Debug log")
//            double secondsElapsed = debugDuration / 1000000000.0;
//            std::sort(frameTimes.begin(), frameTimes.end(), [](const double& lhs, const double& rhs) { return lhs > rhs; });
//            double dtAvg = 0.0;
//            double dtLow50 = 0.0;
//            double dtLow10 = 0.0;
//            double dtLow1 = 0.0;
//            double dtMax = frameTimes.empty() ? 0.0 : frameTimes[0];
//            double fps = frameTimes.size() / secondsElapsed;
//
//            for (int i = 0; i < frameTimes.size(); ++i)
//                dtAvg += frameTimes[i];
//            dtAvg /= glm::max(size_t(1), frameTimes.size());
//
//            for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 2); ++i)
//                dtLow50 += frameTimes[i];
//            dtLow50 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 2));
//
//            for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 10); ++i)
//                dtLow10 += frameTimes[i];
//            dtLow10 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 10));
//
//            for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 100); ++i)
//                dtLow1 += frameTimes[i];
//            dtLow1 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 100));
//
//            double dtAvgCpu = 0.0;
//            for (int i = 0; i < cpuFrameTimes.size(); ++i)
//                dtAvgCpu += cpuFrameTimes[i];
//            dtAvgCpu /= cpuFrameTimes.size();
//
//            printf("%.2f FPS (AVG %.3f msec, AVG-CPU %.3f msec,    MAX %.3f msec 1%%LO %.3f msec, 10%%LO %.3f msec, 50%%LO %.3f msec)\n",
//                   fps, dtAvg, dtAvgCpu, dtMax, dtLow1, dtLow10, dtLow50);
//
//            printf("%f polygons/sec - Average frame rendered %f polygons, %f vertices, %f indices - %f draw calls, %f instances, %.2f msec for draw calls\n",
//                   (double)tempDebugInfo.renderedPolygons / secondsElapsed,
//                   (double)tempDebugInfo.renderedPolygons / (double)frameTimes.size(),
//                   (double)tempDebugInfo.renderedVertices / (double)frameTimes.size(),
//                   (double)tempDebugInfo.renderedIndices / (double)frameTimes.size(),
//                   (double)tempDebugInfo.drawCalls / (double)frameTimes.size(),
//                   (double)tempDebugInfo.drawInstances / (double)frameTimes.size(),
//                   ((double)tempDebugInfo.elapsedDrawNanosCPU / (double)frameTimes.size()) / (1e+6)
//            );
//
//            tempDebugInfo.reset();
//            frameTimes.clear();
//            cpuFrameTimes.clear();
//            lastDebug = now;
//        }

        if (isFrame) {
            // The CPU is idle from this point onward, until the loop restarts another frame.
            Profiler::beginCPU(profileID_CPU_Idle);
        }

        //SDL_Delay(1);
    }
    Profiler::endFrame();

    m_rendering = false;

    Engine::graphics()->getDevice()->waitIdle();

    cleanup();
}

Application* Application::instance() {
    return s_instance;
}

void Application::stop() {
    m_running = false;
}

InputHandler* Application::input() {
    return m_inputHandler;
}

glm::ivec2 Application::getWindowSize() const {
    glm::ivec2 size;
    SDL_GetWindowSize(m_windowHandle, &size.x, &size.y);
    return size;
}

double Application::getWindowAspectRatio() const {
    const glm::ivec2& windowSize = getWindowSize();
    return (double)windowSize.x / (double)windowSize.y;
}

const double& Application::getFramerateLimit() const {
    return m_framerateLimit;
}

void Application::setFramerateLimit(const double& framerateLimit) {
    m_framerateLimit = framerateLimit;
}

bool Application::isViewportInverted() const {
    return true;
}

const std::string& Application::getExecutionDirectory() const {
    return m_executionDirectory;
}

const std::thread::id& Application::getMainThreadId() const {
    return m_mainThreadId;
}

uint64_t Application::getHashedMainThreadId() const {
    return ThreadUtils::getThreadHashedId(m_mainThreadId);
}

