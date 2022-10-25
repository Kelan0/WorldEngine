#include "core/application/InputHandler.h"
#include "core/application/Application.h"
#include "core/util/Profiler.h"


InputHandler::InputHandler(SDL_Window* windowHandle) {
    m_windowHandle = windowHandle;

    for (int i = 0; i < KEYBOARD_SIZE; i++) {
        m_keysDown[i] = false;
        m_keysPressed[i] = false;
        m_keysReleased[i] = false;
    }

    for (int i = 0; i < MOUSE_SIZE; i++) {
        m_mouseDown[i] = false;
        m_mousePressed[i] = false;
        m_mouseReleased[i] = false;
        m_mouseDragged[i] = false;
        m_mousePressPixelCoord[i] = glm::ivec2(0, 0);
        m_mouseDragPixelOrigin[i] = glm::ivec2(0, 0);
    }
    m_currMousePixelCoord = glm::ivec2(0, 0);
    m_prevMousePixelCoord = glm::ivec2(0, 0);
    m_currMousePixelMotion = glm::ivec2(0, 0);
    m_prevMousePixelMotion = glm::ivec2(0, 0);
    m_mouseGrabbed = false;
    m_didWarpMouse = false;
}

InputHandler::~InputHandler() {}

void InputHandler::update() {
    PROFILE_SCOPE("InputHandler::update")
    for (int i = 0; i < KEYBOARD_SIZE; i++) {
        m_keysPressed[i] = false;
        m_keysReleased[i] = false;
    }

    for (int i = 0; i < MOUSE_SIZE; i++) {
        m_mousePressed[i] = false;
        m_mouseReleased[i] = false;
    }

    m_prevMousePixelCoord = m_currMousePixelCoord;
    m_prevMousePixelMotion = m_currMousePixelMotion;
    m_currMousePixelMotion = glm::ivec2(0, 0);

    if (m_mouseGrabbed) {
        this->setMouseScreenCoord(glm::dvec2(0.5, 0.5));
    }

    m_didWarpMouse = false;
}

void InputHandler::processEvent(const SDL_Event* event) {
    PROFILE_SCOPE("InputHandler::processEvent")
    switch (event->type) {
        case SDL_KEYDOWN:
            m_keysDown[event->key.keysym.scancode] = true;
            m_keysPressed[event->key.keysym.scancode] = true;
            break;
        case SDL_KEYUP:
            m_keysDown[event->key.keysym.scancode] = false;
            m_keysReleased[event->key.keysym.scancode] = true;
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_mouseDown[event->button.button] = true;
            m_mousePressed[event->button.button] = true;
            m_mousePressPixelCoord[event->button.button] = glm::ivec2(event->button.x, event->button.y);
            m_mouseDragPixelOrigin[event->button.button] = glm::ivec2(event->button.x, event->button.y);
            m_mouseDragged[event->button.button] = false;
            break;
        case SDL_MOUSEBUTTONUP:
            m_mouseDown[event->button.button] = false;
            m_mouseReleased[event->button.button] = true;
            m_mouseDragged[event->button.button] = false;
            break;
        case SDL_MOUSEMOTION:
            m_currMousePixelCoord = glm::ivec2(event->motion.x, event->motion.y); // why is this not reset to zero?
            m_currMousePixelMotion = glm::ivec2(event->motion.xrel, event->motion.yrel);
            for (int i = 0; i < MOUSE_SIZE; i++)
                m_mouseDragged[i] = m_mouseDown[i];
            break;
    }
}

bool InputHandler::keyDown(uint32_t key) {
    assert(key < KEYBOARD_SIZE);
    return m_keysDown[key];
}

bool InputHandler::keyPressed(uint32_t key) {
    assert(key < KEYBOARD_SIZE);
    return m_keysPressed[key];
}

bool InputHandler::keyReleased(uint32_t key) {
    assert(key < KEYBOARD_SIZE);
    return m_keysReleased[key];
}

bool InputHandler::mouseDown(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_mouseDown[button];
}

bool InputHandler::mousePressed(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_mousePressed[button];
}

bool InputHandler::mouseReleased(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_mouseReleased[button];
}

bool InputHandler::mouseDragged(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_mouseDragged[button];
}

bool InputHandler::isMouseGrabbed() const {
    return m_mouseGrabbed;
}

void InputHandler::setMouseGrabbed(bool grabbed) {
    if (grabbed != m_mouseGrabbed) {
        m_mouseGrabbed = grabbed;
        this->setMouseScreenCoord(glm::dvec2(0.5, 0.5));
        m_prevMousePixelCoord = m_currMousePixelCoord;
        m_prevMousePixelMotion = glm::ivec2(0);
        SDL_ShowCursor(!grabbed);
        SDL_SetRelativeMouseMode(grabbed ? SDL_TRUE : SDL_FALSE);
        this->getRelativeMouseState(); // Pops the warp caused by this grab from the SDL event queue.
    }
}

void InputHandler::toggleMouseGrabbed() {
    this->setMouseGrabbed(!this->isMouseGrabbed());
}

bool InputHandler::setMousePixelCoord(glm::ivec2 coord) {

    if (coord.x < 0 || coord.x >= Application::instance()->getWindowSize().x || coord.y < 0 || coord.y >= Application::instance()->getWindowSize().y) {
        return false;
    }

    int x, y;
    SDL_GetWindowPosition(m_windowHandle, &x, &y);
    SDL_WarpMouseGlobal(x + coord.x, y + coord.y);
    //SDL_WarpMouseInWindow(m_windowHandle, coord.x, coord.y);
    m_currMousePixelCoord = glm::ivec2(coord);
    m_didWarpMouse = true;
    return true;
}

bool InputHandler::setMouseScreenCoord(glm::dvec2 coord) {
    return this->setMousePixelCoord(glm::ivec2(coord * glm::dvec2(Application::instance()->getWindowSize())));
}

bool InputHandler::didWarpMouse() const {
    return m_didWarpMouse;
}

glm::ivec2 InputHandler::getMousePixelCoord() {
    return m_currMousePixelCoord;
}

glm::ivec2 InputHandler::getLastMousePixelCoord() {
    return m_prevMousePixelCoord;
}

glm::dvec2 InputHandler::getMouseScreenCoord() {
    return glm::dvec2(m_currMousePixelCoord) / glm::dvec2(Application::instance()->getWindowSize());
}

glm::dvec2 InputHandler::getLastMouseScreenCoord() {
    return glm::dvec2(m_prevMousePixelCoord) / glm::dvec2(Application::instance()->getWindowSize());
}

glm::ivec2 InputHandler::getMousePixelMotion() {
    return m_currMousePixelMotion;
}

glm::ivec2 InputHandler::getLastMousePixelMotion() {
    return m_prevMousePixelMotion;
}

glm::ivec2 InputHandler::getRelativeMouseState() {
    int x, y;
    SDL_GetRelativeMouseState(&x, &y);
    return glm::ivec2(x, y);
}

glm::dvec2 InputHandler::getMouseScreenMotion() {
    return glm::dvec2(m_currMousePixelMotion) / glm::dvec2(Application::instance()->getWindowSize());
}

glm::dvec2 InputHandler::getLastMouseScreenMotion() {
    return glm::dvec2(m_prevMousePixelMotion) / glm::dvec2(Application::instance()->getWindowSize());
}

glm::ivec2 InputHandler::getMouseDragPixelOrigin(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_mouseDragPixelOrigin[button];
}

glm::ivec2 InputHandler::getMouseDragPixelDistance(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return m_currMousePixelCoord - m_mouseDragPixelOrigin[button];
}

glm::dvec2 InputHandler::getMouseDragScreenOrigin(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return glm::dvec2(m_mouseDragPixelOrigin[button]) / glm::dvec2(Application::instance()->getWindowSize());
}

glm::dvec2 InputHandler::getMouseDragScreenDistance(uint32_t button) {
    assert(button < MOUSE_SIZE);
    return glm::dvec2(m_mouseDragPixelOrigin[button] - m_currMousePixelCoord) / glm::dvec2(Application::instance()->getWindowSize());
}