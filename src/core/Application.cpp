#include "Application.h"
#include "graphics/GraphicsManager.h"
#include "graphics/GraphicsPipeline.h"
#include <chrono>

Application* Application::s_instance = NULL;


Application::Application() {
	m_graphics = new GraphicsManager();
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

bool Application::create() {
	printf("Creating application\n");

	assert(s_instance == NULL);
	s_instance = new Application();

	printf("Initializing SDL\n");
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		// Failed initialization
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		destroy();
		return false;
	}

	printf("Creating window\n");
	uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	s_instance->m_windowHandle = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, flags);
	if (s_instance->m_windowHandle == NULL) {
		// Failed to create window
		printf("Failed to create SDL window: %s\n", SDL_GetError());
		destroy();
		return false;
	}

	if (!s_instance->m_graphics->init(s_instance->m_windowHandle, "WorldEngine")) {
		// Failed to init graphics
		printf("Failed to initialize graphics engine\n");
		destroy();
		return false;
	}

	return true;
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

		vk::CommandBuffer commandBuffer;
		vk::Framebuffer framebuffer;
		if (m_graphics->beginFrame(commandBuffer, framebuffer)) {

			GraphicsPipeline& pipeline = m_graphics->pipeline();

			vk::ClearValue clearValue;
			clearValue.color.setFloat32({ 0.0F, 0.0F, 0.0F, 1.0F });

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.setRenderPass(*pipeline.getRenderPass());
			renderPassBeginInfo.setFramebuffer(framebuffer);
			renderPassBeginInfo.renderArea.setOffset({ 0, 0 });
			renderPassBeginInfo.renderArea.setExtent(m_graphics->getImageExtent());
			renderPassBeginInfo.setClearValueCount(1);
			renderPassBeginInfo.setPClearValues(&clearValue);

			commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
			pipeline.bind(commandBuffer);
			commandBuffer.draw(3, 1, 0, 0);
			commandBuffer.endRenderPass();

			m_graphics->endFrame();
		}

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
