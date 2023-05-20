
#include "core/application/Engine.h"
#include "core/application/Application.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/physics/PhysicsSystem.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/renderPasses/UIRenderer.h"
#include "core/engine/renderer/renderPasses/LightRenderer.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/graphics/GraphicsManager.h"
#include "core/util/Profiler.h"
#include "core/util/Logger.h"
#include <SDL2/SDL.h>


Engine* Engine::s_instance = new Engine();

Engine::Engine():
    m_graphics(new GraphicsManager()),
    m_scene(new Scene()),
    m_physicsSystem(new PhysicsSystem()),
    m_uiRenderer(new UIRenderer()),
    m_sceneRenderer(new SceneRenderer()),
    m_lightRenderer(new LightRenderer()),
    m_immediateRenderer(new ImmediateRenderer()),
    m_deferredRenderer(new DeferredRenderer()),
    m_reprojectionRenderer(new ReprojectionRenderer()),
    m_postProcessingRenderer(new PostProcessRenderer()),
    m_eventDispatcher(new EventDispatcher()),
    m_currentFrameCount(0),
    m_startTime(std::chrono::high_resolution_clock::now()),
    m_accumulatedTime(0.0),
    m_runTime(0.0),
    m_viewFrustumPaused(false),
    m_renderWireframeEnabled(false),
    m_debugCompositeEnabled(true),
    m_renderCamera(new RenderCamera()),
    m_viewFrustum(nullptr) {
}

Engine::~Engine() {
    LOG_INFO("Destroying Engine");

    delete m_scene; // Scene is destroyed before SceneRenderer since destruction of components may interact with SceneRenderer. This might need to change.
    delete m_physicsSystem;
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
    delete m_viewFrustum;
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

PhysicsSystem* Engine::getPhysicsSystem() const {
    return m_physicsSystem;
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

uint64_t Engine::getCurrentFrameCount() const {
    return m_currentFrameCount;
}

bool Engine::isDebugCompositeEnabled() const {
    return m_debugCompositeEnabled;
}

bool Engine::isViewFrustumPaused() const {
    return m_viewFrustumPaused;
}

void Engine::setViewFrustumPaused(bool viewFrustumPaused) {
    m_viewFrustumPaused = viewFrustumPaused;
}

bool Engine::isRenderWireframeEnabled() const {
    return m_renderWireframeEnabled;
}

void Engine::setRenderWireframeEnabled(bool renderWireframeEnabled) {
    m_renderWireframeEnabled = renderWireframeEnabled;
}

double Engine::getPartialFrames() const {
    return Application::instance()->getPartialFrames();
}

double Engine::getPartialTicks() const {
    return Application::instance()->getPartialTicks();
}

double Engine::getAccumulatedTime() {
    return m_accumulatedTime;
}

double Engine::getRunTime() {
    return m_runTime;
}

void Engine::setDebugCompositeEnabled(bool debugCompositeEnabled) {
    m_debugCompositeEnabled = debugCompositeEnabled;
}

const RenderCamera* Engine::getRenderCamera() const {
    return m_renderCamera;
}

const Frustum* Engine::getViewFrustum() const {
    return m_viewFrustum;
}

GraphicsManager* Engine::graphics() {
    return instance()->getGraphics();
}

Scene* Engine::scene() {
    return instance()->getScene();
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
        LOG_ERROR("Failed to initialize graphics engine");
        return false;
    }

    PROFILE_REGION("Init UIRenderer")
    if (!m_uiRenderer->init(windowHandle))
        return false;

    PROFILE_REGION("Init Scene")
    m_scene->init();
    m_eventDispatcher->repeatAll(m_scene->getEventDispatcher());

    PROFILE_REGION("Init PhysicsSystem")
    m_physicsSystem->setScene(m_scene);
    if (!m_physicsSystem->init())
        return false;

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

    m_runTime = 0.0;
    m_accumulatedTime = 0.0;
    m_currentFrameCount = 0;

    m_startTime = std::chrono::high_resolution_clock::now();

    return true;
}

void Engine::preRender(double dt) {
    PROFILE_SCOPE("Engine::preRender");

    m_uiRenderer->preRender(dt);
    m_physicsSystem->preRender(dt);
    m_sceneRenderer->preRender(dt);
    m_lightRenderer->preRender(dt);
    m_deferredRenderer->preRender(dt);
    m_reprojectionRenderer->preRender(dt);
}

void Engine::render(double dt) {
    PROFILE_SCOPE("Engine::render");

//    PROFILE_BEGIN_GPU_TIMESTAMP("Engine::render");

    // Update camera - This ideally should happen within preRender, however the application may change
    // the camera in its own render() method, which won't be updated until the next frame. Updating
    // the camera here fixes that.
    // TODO: we need an input() method for the application to override.
    const Entity& cameraEntity = Engine::scene()->getMainCameraEntity();

    m_renderCamera->setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera->setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera->update();

    if (m_viewFrustum == nullptr) {
        m_viewFrustum = new Frustum(*m_renderCamera);
    } else if (!m_viewFrustumPaused) {
        m_viewFrustum->set(*m_renderCamera);
    }

    auto& commandBuffer = graphics()->getCurrentCommandBuffer();
    PROFILE_BEGIN_GPU_CMD("Engine::render", commandBuffer)

    if (m_currentFrameCount == 0) {
        // Initializes the BRDF integration map on the first frame
        EnvironmentMap::getBRDFIntegrationMap(graphics()->getCurrentCommandBuffer());

        // Initialize empty environment map on the first frame
        EnvironmentMap::getEmptyEnvironmentMap();
    }

    m_lightRenderer->render(dt, commandBuffer, m_renderCamera);

    m_deferredRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->beginGeometrySubpass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->renderGeometryPass(dt, commandBuffer, m_renderCamera, m_viewFrustum);
    m_deferredRenderer->beginLightingSubpass(commandBuffer, vk::SubpassContents::eInline);
    m_deferredRenderer->renderLightingPass(dt, commandBuffer, m_renderCamera, m_viewFrustum);
    commandBuffer.endRenderPass();

    if (m_debugCompositeEnabled) {
        m_immediateRenderer->render(dt, commandBuffer);
    }

    m_reprojectionRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_reprojectionRenderer->render(dt, commandBuffer);
    commandBuffer.endRenderPass();

    m_postProcessingRenderer->updateExposure(dt, commandBuffer);
    m_postProcessingRenderer->renderBloomBlur(dt, commandBuffer);

    m_postProcessingRenderer->beginRenderPass(commandBuffer, vk::SubpassContents::eInline);
    m_postProcessingRenderer->render(dt, commandBuffer);
    commandBuffer.endRenderPass();

    m_uiRenderer->render(dt, commandBuffer);
//    PROFILE_END_GPU_TIMESTAMP("Engine::render");
    PROFILE_END_GPU_CMD("Engine::render", commandBuffer);

    ++m_currentFrameCount;
    m_accumulatedTime += dt;

    // m_runTime accurate to 1/10th millisecond
    constexpr uint32_t res = 10000;
    using dur = std::chrono::duration<long long, std::ratio<1, res>>;
    auto now = std::chrono::high_resolution_clock::now();
    m_runTime = (double)std::chrono::duration_cast<dur>(now - m_startTime).count() / (double)res;
}

void Engine::preTick(double dt) {
    PROFILE_SCOPE("Engine::preTick");
    m_physicsSystem->preTick(dt);
    m_scene->preTick(dt);
}

void Engine::tick(double dt) {
    PROFILE_SCOPE("Engine::tick");
    m_physicsSystem->tick(dt);
}

void Engine::cleanup() {
    LOG_INFO("Engine cleaning up");
    m_graphics->shutdownGraphics();
}

void Engine::destroy() {
    delete Engine::s_instance;
    Engine::s_instance = nullptr;
}