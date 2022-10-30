
#include "core/application/Engine.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/renderPasses/UIRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/util/Profiler.h"
#include <SDL2/SDL.h>


Engine* Engine::s_instance = new Engine();

Engine::Engine():
    m_graphics(new GraphicsManager()),
    m_eventDispatcher(new EventDispatcher()),
    m_scene(new Scene()),
    m_uiRenderer(new UIRenderer()),
    m_sceneRenderer(new SceneRenderer()),
    m_lightRenderer(new LightRenderer()),
    m_immediateRenderer(new ImmediateRenderer()),
    m_reprojectionRenderer(new ReprojectionRenderer()),
    m_deferredRenderer(new DeferredRenderer()),
    m_postProcessingRenderer(new PostProcessRenderer()),
    m_renderCamera(new RenderCamera()),
    m_currentFrameCount(0),
    m_debugCompositeEnabled(true) {
}

Engine::~Engine() {

    delete m_scene; // Scene is destroyed before SceneRenderer since destruction of components may interact with SceneRenderer. This might need to change.
    delete m_uiRenderer;
    delete m_sceneRenderer;
    delete m_lightRenderer;
    delete m_immediateRenderer;
    delete m_reprojectionRenderer;
    delete m_deferredRenderer;
    delete m_postProcessingRenderer;
    delete m_eventDispatcher;
    delete m_graphics;

    delete m_renderCamera;
}

void Engine::processEvent(const SDL_Event* event) {
    PROFILE_SCOPE("Engine::processEvent")
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

DeferredRenderer* Engine::getDeferredRenderer() const {
    return m_deferredRenderer;
}

ReprojectionRenderer* Engine::getReprojectionRenderer() const {
    return m_reprojectionRenderer;
}

PostProcessRenderer* Engine::getPostProcessingRenderer() const {
    return m_postProcessingRenderer;
}

EventDispatcher* Engine::getEventDispatcher() const {
    return m_eventDispatcher;
}

const uint64_t& Engine::getCurrentFrameCount() const {
    return m_currentFrameCount;
}

const bool& Engine::isDebugCompositeEnabled() const {
    return m_debugCompositeEnabled;
}

void Engine::setDebugCompositeEnabled(const bool& debugCompositeEnabled) {
    m_debugCompositeEnabled = debugCompositeEnabled;
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

ReprojectionRenderer* Engine::reprojectionRenderer() {
    return instance()->getReprojectionRenderer();
}

DeferredRenderer* Engine::deferredRenderer() {
    return instance()->getDeferredRenderer();
}

PostProcessRenderer* Engine::postProcessingRenderer() {
    return instance()->getPostProcessingRenderer();
}

EventDispatcher* Engine::eventDispatcher() {
    return instance()->getEventDispatcher();
}

const uint64_t& Engine::currentFrameCount() {
    return instance()->getCurrentFrameCount();
}

const bool& Engine::debugCompositeEnabled() {
    return instance()->isDebugCompositeEnabled();
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

    PROFILE_REGION("Init DeferredRenderer")
    if (!m_deferredRenderer->init())
        return false;

    PROFILE_REGION("Init ReprojectionRenderer")
    if (!m_reprojectionRenderer->init())
        return false;

    PROFILE_REGION("Init PostProcessRenderer")
    if (!m_postProcessingRenderer->init())
        return false;

    m_currentFrameCount = 0;

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
    PROFILE_BEGIN_GPU_CMD("Engine::render", commandBuffer)

    m_lightRenderer->render(dt, commandBuffer, m_renderCamera);

    m_deferredRenderer->preRender(dt);
    m_reprojectionRenderer->preRender(dt);

    m_deferredRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->beginGeometrySubpass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->renderGeometryPass(dt, commandBuffer, m_renderCamera);
    m_deferredRenderer->beginLightingSubpass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->render(dt, commandBuffer);
    commandBuffer.endRenderPass();

    if (m_debugCompositeEnabled) {
        m_immediateRenderer->render(dt, commandBuffer);
    }

    m_reprojectionRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_reprojectionRenderer->render(dt, commandBuffer);
    commandBuffer.endRenderPass();

    m_postProcessingRenderer->renderBloomBlur(dt, commandBuffer);

    m_postProcessingRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_postProcessingRenderer->render(dt, commandBuffer);
    commandBuffer.endRenderPass();

    m_uiRenderer->render(dt, commandBuffer);
//    PROFILE_END_GPU_TIMESTAMP("Engine::render");
    PROFILE_END_GPU_CMD(commandBuffer);

    ++m_currentFrameCount;
}

void Engine::cleanup() {
    m_graphics->shutdownGraphics();
}

void Engine::destroy() {
    delete Engine::s_instance;
    Engine::s_instance = nullptr;
}