#include "Application.h"
#include "../graphics/GraphicsManager.h"
#include "../graphics/GraphicsPipeline.h"
#include "../graphics/Mesh.h"
#include <chrono>



Application* Application::s_instance = NULL;


Application::Application() {
}

Application::~Application() {
	delete m_graphics;

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

	init();
}

void Application::cleanupInternal() {
	cleanup();
}

void Application::destroy() {
	assert(s_instance != NULL);
	delete s_instance;
}

void Application::start() {
	bool running = true;

	uint32_t frameCount = 0;

	auto lastDebug = std::chrono::high_resolution_clock::now();
	//auto lastFrame = std::chrono::high_resolution_clock::now();

	while (running) {
		auto frameStart = std::chrono::high_resolution_clock::now();

		SDL_Event event;

		while (running && SDL_PollEvent(&event)) {

			if (event.type == SDL_QUIT) {
				running = false;
			}
		
		}

		render();

		++frameCount;


		uint64_t debugDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(frameStart - lastDebug).count();

		if (debugDuration >= 1000000000u) {
			double secondsElapsed = debugDuration / 1000000000.0;
			printf("%.2f FPS\n", frameCount / secondsElapsed);
			frameCount = 0;
			lastDebug = frameStart;
		}
	}

	m_graphics->getDevice()->waitIdle();

	cleanup();
}

Application* Application::instance() {
	return s_instance;
}

GraphicsManager* Application::graphics() {
	return m_graphics;
}

glm::ivec2 Application::getWindowSize() const {
	glm::ivec2 size;
	SDL_GetWindowSize(m_windowHandle, &size.x, &size.y);
	return size;
}
