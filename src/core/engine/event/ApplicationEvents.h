#ifndef WORLDENGINE_APPLICATIONEVENTS_H
#define WORLDENGINE_APPLICATIONEVENTS_H

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

struct WindowFocusLostEvent {
    uint8_t _v;
};

struct WindowFocusGainedEvent {
    uint8_t _v;
};

#endif //WORLDENGINE_APPLICATIONEVENTS_H
