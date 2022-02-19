#pragma once

#include "../../../core.h"

struct ScreenResizeEvent {
	glm::uvec2 oldSize;
	glm::uvec2 newSize;
};

struct ScreenShowEvent {
	glm::uvec2 size;
};