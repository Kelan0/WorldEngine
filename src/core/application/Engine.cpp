
#include "core/application/Engine.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/renderPasses/UIRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/util/Profiler.h"
#include <SDL2/SDL.h>

Engine* Engine::s_instance = new Engine();


Engine::Engine() {
    m_graphics = new GraphicsManager();
    m_eventDispatcher = new EventDispatcher();
    m_scene = new Scene();
    m_uiRenderer = new UIRenderer();
    m_sceneRenderer = new SceneRenderer();
    m_lightRenderer = new LightRenderer();
    m_immediateRenderer = new ImmediateRenderer();
    m_deferredGeometryPass = new DeferredGeometryRenderPass();
    m_deferredLightingPass = new DeferredLightingRenderPass(m_deferredGeometryPass);

    m_renderCamera = new RenderCamera();
}

Engine::~Engine() {

    delete m_scene; // Scene is destroyed before SceneRenderer since destruction of components may interact with SceneRenderer. This might need to change.
    delete m_uiRenderer;
    delete m_sceneRenderer;
    delete m_lightRenderer;
    delete m_immediateRenderer;
    delete m_deferredGeometryPass;
    delete m_deferredLightingPass;
    delete m_graphics;
    delete m_eventDispatcher;

    delete m_renderCamera;
}

void Engine::processEvent(const SDL_Event* event) {
    m_uiRenderer->processEvent(event);
}

GraphicsManager* Engine::getGraphics() const {
    return m_graphics;
}

Scene* Engine::getScene() const {
    return m_scene;
}

UIRenderer* Engine::getUIRenderer() const {
    return m_uiRenderer;
}

SceneRenderer* Engine::getSceneRenderer() const {
    return m_sceneRenderer;
}

LightRenderer* Engine::getLightRenderer() const {
    return m_lightRenderer;
}

ImmediateRenderer* Engine::getImmediateRenderer() const {
    return m_immediateRenderer;
}

DeferredGeometryRenderPass* Engine::getDeferredGeometryPass() const {
    return m_deferredGeometryPass;
}

DeferredLightingRenderPass* Engine::getDeferredLightingPass() const {
    return m_deferredLightingPass;
}

EventDispatcher* Engine::getEventDispatcher() const {
    return m_eventDispatcher;
}

GraphicsManager* Engine::graphics() {
    return instance()->getGraphics();
}

Scene* Engine::scene() {
    return instance()->getScene();
}

UIRenderer *Engine::uiRenderer() {
    return instance()->getUIRenderer();
}

SceneRenderer* Engine::sceneRenderer() {
    return instance()->getSceneRenderer();
}

LightRenderer* Engine::lightRenderer() {
    return instance()->getLightRenderer();
}

ImmediateRenderer* Engine::immediateRenderer() {
    return instance()->getImmediateRenderer();
}

DeferredGeometryRenderPass* Engine::deferredGeometryPass() {
    return instance()->getDeferredGeometryPass();
}

DeferredLightingRenderPass* Engine::deferredLightingPass() {
    return instance()->getDeferredLightingPass();
}

EventDispatcher* Engine::eventDispatcher() {
    return instance()->getEventDispatcher();
}

Engine* Engine::instance() {
    return s_instance;
}

bool Engine::init(SDL_Window* windowHandle) {
    PROFILE_SCOPE("Engine::init")

    PROFILE_REGION("Init GraphicsManager")

    if (!m_graphics->init(windowHandle, "WorldEngine")) {
        printf("Failed to initialize graphics engine\n");
        return false;
    }

    PROFILE_REGION("Init UIRenderer")
    if (!m_uiRenderer->init(windowHandle))
        return false;

    PROFILE_REGION("Init Scene")
    m_scene->init();
    m_eventDispatcher->repeatAll(m_scene->getEventDispatcher());

    PROFILE_REGION("Init SceneRenderer")
    m_sceneRenderer->setScene(m_scene);
    if (!m_sceneRenderer->init())
        return false;

    PROFILE_REGION("Init LightRenderer")
    if (!m_lightRenderer->init())
        return false;

    PROFILE_REGION("Init ImmediateRenderer")
    if (!m_immediateRenderer->init())
        return false;

    PROFILE_REGION("Init DeferredRenderer geometry pass")
    if (!m_deferredGeometryPass->init())
        return false;

    PROFILE_REGION("Init DeferredRenderer lighting pass")
    if (!m_deferredLightingPass->init())
        return false;

    return true;
}

void Engine::preRender(const double& dt) {
    PROFILE_SCOPE("Engine::preRender");
    m_uiRenderer->preRender(dt);
}

void Engine::render(const double& dt) {
    PROFILE_SCOPE("Engine::render");

//    PROFILE_BEGIN_GPU_TIMESTAMP("Engine::render");
    const Entity& cameraEntity = Engine::scene()->getMainCameraEntity();

    m_renderCamera->setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera->setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera->update();

    m_sceneRenderer->preRender(dt);
    m_lightRenderer->preRender(dt);

    auto& commandBuffer = graphics()->getCurrentCommandBuffer();
    BEGIN_CMD_LABEL(commandBuffer, "Engine::render");

    m_lightRenderer->render(dt, commandBuffer, m_renderCamera);

    m_deferredGeometryPass->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredGeometryPass->render(dt, commandBuffer, m_renderCamera);

//    m_immediateRenderer->render(dt);

    commandBuffer.endRenderPass();

    m_deferredLightingPass->renderScreen(dt);

    m_uiRenderer->render(dt, commandBuffer);
//    PROFILE_END_GPU_TIMESTAMP("Engine::render");
    END_CMD_LABEL(commandBuffer);
}

void Engine::destroy() {
    delete Engine::s_instance;
    Engine::s_instance = nullptr;
}