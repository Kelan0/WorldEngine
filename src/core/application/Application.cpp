#include "Application.h"
#include "InputHandler.h"
#include "../engine/scene/Scene.h"
#include "../engine/renderer/SceneRenderer.h"
#include "../graphics/GraphicsManager.h"
#include "../graphics/GraphicsPipeline.h"
#include "../graphics/Mesh.h"
#include "../engine/scene/event/EventDispacher.h"
#include "../engine/scene/event/Events.h"
#include <chrono>



Application* Application::s_instance = NULL;


Application::Application():
	m_windowHandle(NULL),
	m_graphics(NULL),
	m_inputHandler(NULL), 
	m_scene(NULL),
	m_sceneRenderer(NULL),
	m_eventDispacher(NULL),
	m_running(false) {
}

Application::~Application() {
	delete m_sceneRenderer;
	delete m_scene;
	delete m_inputHandler;
	delete m_graphics;
	delete m_eventDispacher;

	printf("Destroying window\n");
	SDL_DestroyWindow(m_windowHandle);

	printf("Quitting SDL\n");
	SDL_Quit();

	s_instance = NULL;
	printf("Uninitialized application\n");
}

bool Application::initInternal() {

	printf("Initializing SDL\n");
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		return false;
	}

	printf("Creating window\n");
	uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	m_windowHandle = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
	if (m_windowHandle == NULL) {
		printf("Failed to create SDL window: %s\n", SDL_GetError());
		return false;
	}

	m_graphics = new GraphicsManager();
	if (!m_graphics->init(m_windowHandle, "WorldEngine")) {
		printf("Failed to initialize graphics engine\n");
		return false;
	}

	m_eventDispacher = new EventDispacher();

	m_inputHandler = new InputHandler(m_windowHandle);

	m_scene = new Scene();
	m_scene->init();

	m_eventDispacher->repeatAll(m_scene->getEventDispacher());

	m_sceneRenderer = new SceneRenderer();
	m_sceneRenderer->setScene(m_scene);
	m_sceneRenderer->init();

	init();
}

void Application::cleanupInternal() {
	cleanup();
}

void Application::renderInternal(double dt) {


	auto& commandBuffer = graphics()->getCurrentCommandBuffer();
	auto& framebuffer = graphics()->getCurrentFramebuffer();

	std::array<vk::ClearValue, 2> clearValues;
	clearValues[0].color.setFloat32({ 0.0F, 0.0F, 0.25F, 1.0F });
	clearValues[1].depthStencil.setDepth(1.0F).setStencil(0);

	vk::RenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.setRenderPass(graphics()->pipeline()->getRenderPass());
	renderPassBeginInfo.setFramebuffer(framebuffer);
	renderPassBeginInfo.renderArea.setOffset({ 0, 0 });
	renderPassBeginInfo.renderArea.setExtent(graphics()->getImageExtent());
	renderPassBeginInfo.setClearValues(clearValues);

	commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
	render(dt);
	m_sceneRenderer->render(dt);
	commandBuffer.endRenderPass();
}

void Application::processEventsInternal() {
	m_inputHandler->update();

	glm::ivec2 windowSize = getWindowSize();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_QUIT:
			stop();
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_SHOWN:
				m_eventDispacher->trigger(ScreenShowEvent{ getWindowSize() });
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				m_eventDispacher->trigger(ScreenResizeEvent{ windowSize, getWindowSize() });
				break;
			}
			break;
		}

		m_inputHandler->processEvent(event);
	}

	if (m_graphics->didResolutionChange()) {
		printf("Resolution changed\n");
	}
}

void Application::destroy() {
	assert(s_instance != NULL);
	delete s_instance;
}

void Application::start() {
	m_running = true;

	// Trigger a ScreenResizeEvent at the beginning of the render loop so that anything that needs it can be initialized easily
	m_eventDispacher->trigger(ScreenResizeEvent{ getWindowSize(), getWindowSize() }); 

	std::vector<double> frameTimes;
	std::vector<double> cpuFrameTimes;

	auto lastDebug = std::chrono::high_resolution_clock::now();
	auto lastFrame = std::chrono::high_resolution_clock::now();
	auto lastTime = std::chrono::high_resolution_clock::now();

	double partialFrames = 0.0;

	DebugUtils::RenderInfo tempDebugInfo;

	while (m_running) {
		auto now = std::chrono::high_resolution_clock::now();
		uint64_t elapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();
		lastTime = now;

		bool uncappedFramerate = false;
		double frameDurationNanos = uncappedFramerate ? 1.0 : (1e+9 / 144.0);

		partialFrames += elapsedNanos / frameDurationNanos;

		if (partialFrames >= 1.0) {
			partialFrames = 0.0;

			auto beginFrame = now;

			processEventsInternal();

			if (graphics()->beginFrame()) {
				uint64_t frameElapsedNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastFrame).count();
				double dt = frameElapsedNanos / 1e+9;

				auto cpuBegin = std::chrono::high_resolution_clock::now();
				renderInternal(dt);
				auto cpuEnd = std::chrono::high_resolution_clock::now();

				graphics()->endFrame();

				tempDebugInfo += graphics()->debugInfo();

				lastFrame = now;

				auto endFrame = std::chrono::high_resolution_clock::now();
				frameTimes.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(endFrame - beginFrame).count() / 1000000.0);
				cpuFrameTimes.emplace_back(std::chrono::duration_cast<std::chrono::nanoseconds>(cpuEnd - cpuBegin).count() / 1000000.0);
			}
		}


		uint64_t debugDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastDebug).count();

		if (debugDuration >= 1000000000u) {
			double secondsElapsed = debugDuration / 1000000000.0;
			std::sort(frameTimes.begin(), frameTimes.end(), [](const double& lhs, const double& rhs) { return lhs > rhs; });
			double dtAvg = 0.0;
			double dtLow50 = 0.0;
			double dtLow10 = 0.0;
			double dtLow1 = 0.0;
			double dtMax = frameTimes.empty() ? 0.0 : frameTimes[0];
			double fps = frameTimes.size() / secondsElapsed;

			for (int i = 0; i < frameTimes.size(); ++i)
				dtAvg += frameTimes[i];
			dtAvg /= glm::max(size_t(1), frameTimes.size());

			for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 2); ++i)
				dtLow50 += frameTimes[i];
			dtLow50 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 2));

			for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 10); ++i)
				dtLow10 += frameTimes[i];
			dtLow10 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 10));

			for (int i = 0; i < INT_DIV_CEIL(frameTimes.size(), 100); ++i)
				dtLow1 += frameTimes[i];
			dtLow1 /= glm::max(size_t(1), INT_DIV_CEIL(frameTimes.size(), 100));

			double dtAvgCpu = 0.0;
			for (int i = 0; i < cpuFrameTimes.size(); ++i)
				dtAvgCpu += cpuFrameTimes[i];
			dtAvgCpu /= cpuFrameTimes.size();

			printf("%.2f FPS (AVG %.3f msec, AVG-CPU %.3f msec,    MAX %.3f msec 1%%LO %.3f msec, 10%%LO %.3f msec, 50%%LO %.3f msec)\n", 
				fps, dtAvg, dtAvgCpu, dtMax, dtLow1, dtLow10, dtLow50);

			printf("%f polygons/sec - Average frame rendered %f polygons, %f vertices, %f indices - %f draw calls, %f instances, %.2f msec for draw calls\n",
				(double)tempDebugInfo.renderedPolygons / secondsElapsed,
				(double)tempDebugInfo.renderedPolygons / (double)frameTimes.size(),
				(double)tempDebugInfo.renderedVertices / (double)frameTimes.size(),
				(double)tempDebugInfo.renderedIndices / (double)frameTimes.size(),
				(double)tempDebugInfo.drawCalls / (double)frameTimes.size(),
				(double)tempDebugInfo.drawInstances / (double)frameTimes.size(),
				((double)tempDebugInfo.elapsedDrawNanosCPU / (double)frameTimes.size()) / (1e+6)
			);

			tempDebugInfo.reset();
			frameTimes.clear();
			lastDebug = now;
		}

		//SDL_Delay(1);
	}

	m_graphics->getDevice()->waitIdle();

	cleanup();
}

Application* Application::instance() {
	return s_instance;
}

void Application::stop() {
	m_running = false;
}

GraphicsManager* Application::graphics() {
	return m_graphics;
}

InputHandler* Application::input() {
	return m_inputHandler;
}

Scene* Application::scene() {
	return m_scene;
}

SceneRenderer* Application::renderer() {
	return m_sceneRenderer;
}

EventDispacher* Application::eventDispacher() {
	return m_eventDispacher;
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

bool Application::isViewportInverted() const {
	return true;
}
