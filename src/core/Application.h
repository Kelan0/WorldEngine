#pragma once

#include "core.h"
#include <SDL.h>

class GraphicsManager;

class Application {
	NO_COPY(Application);
private:
	Application();

	~Application();

public:
	static bool create();

	static void destroy();

	void start();

	static Application* instance();

	GraphicsManager* graphics();

	glm::ivec2 getWindowSize() const;
private:
	static Application* s_instance;

	SDL_Window* m_windowHandle;
	GraphicsManager* m_graphics;
};

