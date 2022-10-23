
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

struct ScreenHiddenEvent {
    uint8_t _v;
};

struct ScreenMinimisedEvent {
    uint8_t _v;
};

struct ScreenMaximisedEvent {
    uint8_t _v;
};

struct RecreateSwapchainEvent {
    uint8_t _v;
};

struct ShutdownGraphicsEvent {
    uint8_t _v;
};

#endif //WORLDENGINE_EVENTS_H
