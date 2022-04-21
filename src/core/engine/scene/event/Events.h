
#ifndef WORLDENGINE_EVENTS_H
#define WORLDENGINE_EVENTS_H

#include "core/core.h"

struct ScreenResizeEvent {
    glm::uvec2 oldSize;
    glm::uvec2 newSize;
};

struct ScreenShowEvent {
    glm::uvec2 size;
};

struct RecreateSwapchainEvent {
    uint8_t _v;
};

#endif //WORLDENGINE_EVENTS_H
