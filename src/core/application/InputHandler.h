
#ifndef WORLDENGINE_INPUTHANDLER_H
#define WORLDENGINE_INPUTHANDLER_H

#include "../core.h"
#include <SDL.h>

#define KEYBOARD_SIZE 256
#define MOUSE_SIZE 16

class InputHandler {
public:
    InputHandler(SDL_Window* windowHandle);

    ~InputHandler();

    void update();

    void processEvent(SDL_Event event);

    bool keyDown(uint32_t key);

    bool keyPressed(uint32_t key);

    bool keyReleased(uint32_t key);

    bool mouseDown(uint32_t button);

    bool mousePressed(uint32_t button);

    bool mouseReleased(uint32_t button);

    bool mouseDragged(uint32_t button);

    bool isMouseGrabbed() const;

    void setMouseGrabbed(bool grabbed);

    void toggleMouseGrabbed();

    bool setMousePixelCoord(glm::ivec2 coord);

    bool setMouseScreenCoord(glm::dvec2 coord);

    bool didWarpMouse() const;

    glm::ivec2 getMousePixelCoord();

    glm::ivec2 getLastMousePixelCoord();

    glm::dvec2 getMouseScreenCoord();

    glm::dvec2 getLastMouseScreenCoord();

    glm::ivec2 getMousePixelMotion();

    glm::ivec2 getLastMousePixelMotion();

    glm::ivec2 getRelativeMouseState();

    glm::dvec2 getMouseScreenMotion();

    glm::dvec2 getLastMouseScreenMotion();

    glm::ivec2 getMouseDragPixelOrigin(uint32_t button);

    glm::ivec2 getMouseDragPixelDistance(uint32_t button);

    glm::dvec2 getMouseDragScreenOrigin(uint32_t button);

    glm::dvec2 getMouseDragScreenDistance(uint32_t button);

private:
    SDL_Window* m_windowHandle;

    bool m_keysDown[KEYBOARD_SIZE];
    bool m_keysPressed[KEYBOARD_SIZE];
    bool m_keysReleased[KEYBOARD_SIZE];
    bool m_mouseDown[MOUSE_SIZE];
    bool m_mousePressed[MOUSE_SIZE];
    bool m_mouseReleased[MOUSE_SIZE];
    bool m_mouseDragged[MOUSE_SIZE];
    glm::ivec2 m_mousePressPixelCoord[MOUSE_SIZE];
    glm::ivec2 m_mouseDragPixelOrigin[MOUSE_SIZE];
    glm::ivec2 m_currMousePixelCoord;
    glm::ivec2 m_prevMousePixelCoord;
    glm::ivec2 m_currMousePixelMotion;
    glm::ivec2 m_prevMousePixelMotion;
    bool m_mouseGrabbed;
    bool m_didWarpMouse;
};

#endif //WORLDENGINE_INPUTHANDLER_H
