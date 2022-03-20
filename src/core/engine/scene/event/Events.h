
#ifndef WORLDENGINE_EVENTS_H
#define WORLDENGINE_EVENTS_H

#include "../../../core.h"

struct ScreenResizeEvent {
    glm::uvec2 oldSize;
    glm::uvec2 newSize;
};

struct ScreenShowEvent {
    glm::uvec2 size;
};

#endif //WORLDENGINE_EVENTS_H
