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
#include "core/util/Logger.h"
#include "core/util/Util.h"
#include <chrono>
#include <filesystem>

Application* Application::s_instance = nullptr;


Application::Application():
        m_logger(new Logger()),
        m_framerateLimit(0.0), // Unlimited
        m_tickrate(60.0),
        m_partialFrames(0.0),
        m_partialTicks(0.0),
        m_windowHandle(nullptr),
        m_inputHandler(nullptr),
        m_running(false),
        m_rendering(false),
        m_shutdown(false) {
}

Application::~Application() {
    delete m_inputHandler;

    LOG_INFO("Destroying window");
    SDL_DestroyWindow(m_windowHandle);

    LOG_INFO("Quitting SDL");
    SDL_Quit();

    LOG_INFO("Uninitialized application");

    s_instance = nullptr;
    delete m_logger;
}

bool Application::parseArgs(int argc, char* argv[]) {
    assert(argc > 0); // First argument is the executable directory
    m_executionDirectory = PlatformUtils::getFileDirectory(argv[0]);

    m_resourceDirectory = "res/";
    m_shaderCompilerDirectory = "";

    char* value;
    for (int i = 1; i < argc; ++i) {
        value = argv[i];

        if (getArgValue(argc, argv, i, { "--resdir" }, value)) {
            m_resourceDirectory = value;
        } else if (getArgValue(argc, argv, i, { "--spvcdir" }, value)) {
            m_shaderCompilerDirectory = value;
        }
    }

    m_resourceDirectory = PlatformUtils::getAbsoluteFilePath(m_resourceDirectory);
    m_shaderCompilerDirectory = PlatformUtils::getAbsoluteFilePath(m_shaderCompilerDirectory);

    return true;
}

bool Application::matchesArgWithValue(int argc, char* argv[], int index, const std::vector<const char*>& argNames) {
    if (index + 1 >= argc) {
        return false;
    }

    const char* arg = argv[index];

    for (const char* argName : argNames) {
        if (strcmp(arg, argName) == 0) {
            return true;
        }
    }

    return false;
}

bool Application::getArgValue(int argc, char* argv[], int& index, const std::vector<const char*>& argNames, char*& outValue) {
    if (matchesArgWithValue(argc, argv, index, argNames)) {
        ++index;
        outValue = argv[index];

        size_t len = strlen(outValue);
        size_t i0 = std::string::npos;
        size_t i1 = std::string::npos;

        if (len > 0) {
            for (size_t i = 0; i < len && i0 != std::string::npos; ++i) {
                if (outValue[i] == '"')
                    i0 = i + 1;
            }
            for (size_t i = len - 1; i >= 0 && i1 != std::string::npos; --i) {
                if (outValue[i] == '"')
                    i1 = i;
            }
        }

        if (i0 != i1) {
            if (i0 == std::string::npos || i1 == std::string::npos || i0 > i1) {
                LOG_INFO("Mismatched argument value quotation marks");
                return false;
            }

            outValue[i1] = '\0'; // End the string at the last quotation mark
            outValue += i0; // Shift the start of the string to within the first quotation mark.
        }
        return true;
    }
    return false;
}

bool Application::initInternal() {
    PROFILE_SCOPE("Application::initInternal")

    m_mainThreadId = std::this_thread::get_id();
    LOG_INFO("Initializing application on main thread 0x%016llx", ThreadUtils::getThreadHashedId(m_mainThreadId));

    PROFILE_REGION("Init SDL")

    LOG_INFO("Initializing SDL");
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        LOG_ERROR("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }


    LOG_INFO("Creating window");
    uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    m_windowHandle = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
    if (m_windowHandle == nullptr) {
        LOG_ERROR("Failed to create SDL window: %s", SDL_GetError());
        return false;
    }


    PROFILE_REGION("Init InputHandler")
    m_inputHandler = new InputHandler(m_windowHandle);

    if (!Engine::instance()->init(m_windowHandle)) {
        LOG_ERROR("Failed to initialize engine, unable to continue");
        return false;
    }

    PROFILE_REGION("Init Application")
    init();

    return true;
}

void Application::cleanupInternal() {
    Engine::instance()->cleanup();

    LOG_INFO("Application cleaning up");
    cleanup();
    LOG_INFO("Cleanup done");
}

void Application::renderInternal(double dt) {
    PROFILE_SCOPE("Application::renderInternal");

    Engine::instance()->preRender(dt);

    PROFILE_REGION("Application::render - Implementation")
    render(dt);
    PROFILE_END_REGION()

    Engine::instance()->render(dt);

}

void Application::tickInternal(double dt) {
    PROFILE_SCOPE("Application::tickInternal");

    Engine::instance()->preTick(dt);

    PROFILE_REGION("Application::tick - Implementation")
    tick(dt);
    PROFILE_END_REGION()

    Engine::instance()->tick(dt);
}

void Application::processEventsInternal() {
    PROFILE_SCOPE("Application::processEventsInternal")
    m_inputHandler->update();

    glm::ivec2 windowSize = getWindowSize();

    PROFILE_REGION("Poll All Events");

    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        PROFILE_REGION("Handle Event")
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

        PROFILE_END_REGION()
    }

    if (Engine::graphics()->didResolutionChange()) {
        LOG_DEBUG("Resolution changed");
    }
}

void Application::destroy() {
    assert(s_instance != nullptr);
    s_instance->shutdownNow();
    Engine::destroy();
    delete Application::s_instance;
    Application::s_instance = nullptr;

    std::cout << "Goodbye :(" << std::endl;
}

void Application::start() {
    m_running = true;

    m_updateThread = std::thread(&Application::runUpdateThread, this);

    // Trigger a ScreenResizeEvent at the beginning of the render loop so that anything that needs it can be initialized easily
    ScreenResizeEvent event{getWindowSize(), getWindowSize() };
    Engine::eventDispatcher()->trigger(&event);

    std::vector<double> frameTimes;
    std::vector<double> cpuFrameTimes;

    auto lastDebug = std::chrono::high_resolution_clock::now();
    auto lastFrame = std::chrono::high_resolution_clock::now();
    auto lastTime = std::chrono::high_resolution_clock::now();

    m_partialFrames = 0.0;

    DebugUtils::RenderInfo tempDebugInfo;

    static profile_id profileID_CPU_Idle = Profiler::id("CPU Idle");

    try {

        Profiler::beginFrame();
        Profiler::beginCPU(profileID_CPU_Idle);

        while (m_running) {
            auto now = std::chrono::high_resolution_clock::now();
            uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();
            lastTime = now;

            const double framerateLimit = m_framerateLimit < 1.0 ? 1000.0 : m_framerateLimit;

            bool isFrame = false;
            double frameDurationNanos = 1e+9 / framerateLimit;

            m_partialFrames += (double)elapsedNanos / frameDurationNanos;

            Engine::eventDispatcher()->update();

            if (m_partialFrames >= 1.0) {
                Profiler::endCPU(); // profileID_CPU_Idle
                Profiler::endFrame();
                Profiler::beginFrame();
                isFrame = true;

                m_partialFrames = 0.0; // Reset partial frames

                auto beginFrame = now;

                ThreadUtils::wakeThreads();

                processEventsInternal();

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


//            uint64_t debugDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDebug).count();
//
//            if (debugDuration >= 1000000000u) {
//                PROFILE_SCOPE("Debug log")
//                double secondsElapsed = debugDuration / 1000000000.0;
//                std::sort(frameTimes.begin(), frameTimes.end(), [](const double& lhs, const double& rhs) { return lhs > rhs; });
//                double dtAvg = 0.0;
//                double dtLow50 = 0.0;
//                double dtLow10 = 0.0;
//                double dtLow1 = 0.0;
//                double dtMax = frameTimes.empty() ? 0.0 : frameTimes[0];
//                double fps = frameTimes.size() / secondsElapsed;
//
//                for (int i = 0; i < frameTimes.size(); ++i)
//                    dtAvg += frameTimes[i];
//                dtAvg /= glm::max(size_t(1), frameTimes.size());
//
//                for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 2); ++i)
//                    dtLow50 += frameTimes[i];
//                dtLow50 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 2));
//
//                for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 10); ++i)
//                    dtLow10 += frameTimes[i];
//                dtLow10 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 10));
//
//                for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 100); ++i)
//                    dtLow1 += frameTimes[i];
//                dtLow1 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 100));
//
//                double dtAvgCpu = 0.0;
//                for (int i = 0; i < cpuFrameTimes.size(); ++i)
//                    dtAvgCpu += cpuFrameTimes[i];
//                dtAvgCpu /= cpuFrameTimes.size();
//
//                LOG_DEBUG("%.2f FPS (AVG %.3f msec, AVG-CPU %.3f msec,    MAX %.3f msec 1%%LO %.3f msec, 10%%LO %.3f msec, 50%%LO %.3f msec)",
//                       fps, dtAvg, dtAvgCpu, dtMax, dtLow1, dtLow10, dtLow50);
//
//                LOG_DEBUG("%f polygons/sec - Average frame rendered %f polygons, %f vertices, %f indices - %f draw calls, %f instances, %.2f msec for draw calls",
//                       (double)tempDebugInfo.renderedPolygons / secondsElapsed,
//                       (double)tempDebugInfo.renderedPolygons / (double)frameTimes.size(),
//                       (double)tempDebugInfo.renderedVertices / (double)frameTimes.size(),
//                       (double)tempDebugInfo.renderedIndices / (double)frameTimes.size(),
//                       (double)tempDebugInfo.drawCalls / (double)frameTimes.size(),
//                       (double)tempDebugInfo.drawInstances / (double)frameTimes.size(),
//                       ((double)tempDebugInfo.elapsedDrawNanosCPU / (double)frameTimes.size()) / (1e+6)
//                );
//
//                tempDebugInfo.reset();
//                frameTimes.clear();
//                cpuFrameTimes.clear();
//                lastDebug = now;
//            }

            if (isFrame) {
                // The CPU is idle from this point onward, until the loop restarts another frame.
                Profiler::beginCPU(profileID_CPU_Idle);
            }

//            SDL_Delay(1);
        }
        Profiler::endFrame();

    } catch (const std::exception& e) {
        LOG_ERROR("Caught exception:\n%s", e.what());
    } catch (const std::string& e) {
        LOG_ERROR("Caught exception:\n%s", e.c_str());
    }

    shutdownNow();
}

void Application::shutdownNow() {
    if (!m_shutdown) {
        m_shutdown = true;

        LOG_INFO("Application shutting down");

        m_running = false;
        m_rendering = false;

        Engine::graphics()->getDevice()->waitIdle();

        if (m_updateThread.joinable())
            m_updateThread.join(); // Wait for update thread to shut down

        cleanupInternal();
    }
}

void Application::runUpdateThread() {
    m_updateThreadId = std::this_thread::get_id();
    assert(m_mainThreadId != m_updateThreadId);
    assert(m_running);
    assert(m_tickrate >= 1.0);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastTime = std::chrono::high_resolution_clock::now();

    m_partialTicks = 0.0;

    double tickDeltaTime = 1.0 / m_tickrate; // Tick delta time is constant. Variation would cause unstable physics simulation
    double tickDurationNanos = 1e+9 / m_tickrate;

    double simulationTime = 0.0;

    while (m_running) {
        auto now = std::chrono::high_resolution_clock::now();
        uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();
        lastTime = now;

        m_partialTicks += (double)elapsedNanos / tickDurationNanos;

        if (m_partialTicks >= 1.0) {
            m_partialTicks -= 1.0; // Decrement one tick

            tickInternal(tickDeltaTime);

            simulationTime += tickDeltaTime;

            if (m_partialTicks >= m_tickrate * 5) {
                uint64_t realElapsedSimTimeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                double missedSimTimeMsec = realElapsedSimTimeMsec - (simulationTime * 1000.0);
                LOG_WARN("Simulation thread can't keep up. Skipping %llu ticks (Simulation is running %.2f msec behind)", (uint64_t)m_partialTicks, missedSimTimeMsec);
                m_partialTicks = 0.0;
            }
        }
    }
}

Application* Application::instance() {
    return s_instance;
}

void Application::stop() {
    m_running = false;
}

Logger* Application::logger() {
    return m_logger;
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

double Application::getFramerateLimit() const {
    return m_framerateLimit;
}

void Application::setFramerateLimit(double framerateLimit) {
    m_framerateLimit = framerateLimit;
}

double Application::getTickrate() const {
    return m_tickrate;
}

void Application::setTickrate(double tickrate) {
    if (m_running) {
        LOG_ERROR("Cannot change tickrate while running");
        assert(false);
        return;
    }
    m_tickrate = tickrate;
}

double Application::getPartialFrames() const {
    return m_partialFrames;
}

double Application::getPartialTicks() const {
    return m_partialTicks;
}

bool Application::isViewportInverted() const {
    return true;
}

const std::string& Application::getExecutionDirectory() const {
    return m_executionDirectory;
}

const std::string& Application::getResourceDirectory() const {
    return m_resourceDirectory;
}

const std::string& Application::getShaderCompilerDirectory() const {
    return m_shaderCompilerDirectory;
}

std::string Application::getAbsoluteResourceFilePath(const std::string& resourceFilePath) const {
    auto path = std::filesystem::path(resourceFilePath);
    if (path.is_absolute()) {
        std::string s = path.string();
        return s;
    }
    return std::filesystem::absolute(getResourceDirectory() + resourceFilePath).string();
}

const std::thread::id& Application::getMainThreadId() const {
    return m_mainThreadId;
}

uint64_t Application::getHashedMainThreadId() const {
    return ThreadUtils::getThreadHashedId(m_mainThreadId);
}

