#include "TerrainTestApplication.h"
#include "core/graphics/ImageCube.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/scene/terrain/QuadtreeTerrain.h"
#include "core/engine/scene/Scene.h"
#include "core/application/InputHandler.h"
#include "core/engine/scene/EntityHierarchy.h"

TerrainTestApplication::TerrainTestApplication() {

}

TerrainTestApplication::~TerrainTestApplication() {

}

void TerrainTestApplication::init() {

    setTickrate(60.0);

    ImageCubeConfiguration imageCubeConfig{};
    imageCubeConfig.device = Engine::graphics()->getDevice();
    imageCubeConfig.format = vk::Format::eR32G32B32A32Sfloat;
    imageCubeConfig.usage = vk::ImageUsageFlagBits::eSampled;
    imageCubeConfig.generateMipmap = true;
    imageCubeConfig.mipLevels = UINT32_MAX;
    imageCubeConfig.imageSource.setEquirectangularSource("environment_maps/rustig_koppie_puresky_8k.hdr");
    std::shared_ptr<ImageCube> skyboxImageCube = std::shared_ptr<ImageCube>(ImageCube::create(imageCubeConfig, "SkyboxCubeImage"));

    std::shared_ptr<EnvironmentMap> skyboxEnvironmentMap = std::make_shared<EnvironmentMap>(skyboxImageCube);
    skyboxEnvironmentMap->update();

    Engine::instance()->getDeferredRenderer()->setEnvironmentMap(skyboxEnvironmentMap);

    Entity terrainEntity = EntityHierarchy::create(Engine::scene(), "terrainEntity");
    terrainEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
    terrainEntity.addComponent<QuadtreeTerrainComponent>();
}

void TerrainTestApplication::cleanup() {

}

void TerrainTestApplication::render(double dt) {
    PROFILE_SCOPE("TerrainTestApplication::render")
    handleUserInput(dt);
}

void TerrainTestApplication::tick(double dt) {

}

void TerrainTestApplication::handleUserInput(double dt) {
    if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
        input()->toggleMouseGrabbed();
    }

    if (input()->keyPressed(SDL_SCANCODE_F2)) {
        Engine::instance()->setRenderWireframeEnabled(!Engine::instance()->isRenderWireframeEnabled());
    }

    if (input()->isMouseGrabbed()) {
        Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();
        glm::ivec2 dMouse = input()->getRelativeMouseState();
        if (isViewportInverted())
            dMouse.y *= -1;

        const float mouseSensitivity = 0.001F / currentZoomFactor;
        cameraPitch += dMouse.y * mouseSensitivity;
        cameraYaw -= dMouse.x * mouseSensitivity;
        if (cameraYaw > +M_PI) cameraYaw -= M_PI * 2.0;
        if (cameraYaw < -M_PI) cameraYaw += M_PI * 2.0;
        if (cameraPitch > +M_PI * 0.5) cameraPitch = +M_PI * 0.5;
        if (cameraPitch < -M_PI * 0.5) cameraPitch = -M_PI * 0.5;

        cameraTransform.setRotation((float) cameraPitch, (float) cameraYaw);

        glm::dvec3 movementDir(0.0);

        if (input()->keyDown(SDL_SCANCODE_W)) movementDir.z--;
        if (input()->keyDown(SDL_SCANCODE_S)) movementDir.z++;
        if (input()->keyDown(SDL_SCANCODE_A)) movementDir.x--;
        if (input()->keyDown(SDL_SCANCODE_D)) movementDir.x++;
        if (input()->keyDown(SDL_SCANCODE_LSHIFT)) movementDir.y--;
        if (input()->keyDown(SDL_SCANCODE_SPACE)) movementDir.y++;

        if (glm::dot(movementDir, movementDir) > 0.5F) {
            movementDir = cameraTransform.getRotationMatrix() * glm::normalize(movementDir);
            cameraTransform.translate(movementDir * (double)playerMovementSpeed * dt);
        }

        if (input()->keyDown(SDL_SCANCODE_LCTRL)) {
            int zoomIncr = input()->getMouseScrollAmount().y > 0 ? 1 : input()->getMouseScrollAmount().y < 0 ? -1 : 0;
            if (zoomIncr != 0)
                targetZoomFactor *= glm::pow(1.15F, (float)zoomIncr);
        } else {
            targetZoomFactor = 1.0F;
        }
    } else {
        targetZoomFactor = 1.0F;
    }

    if (targetZoomFactor < 0.66F)
        targetZoomFactor = 0.66F;

    if (targetZoomFactor > 1500.0F)
        targetZoomFactor = 1500.0F;

    float zoomInSpeed = 10.0F;
    float zoomOutSpeed = 10.0F;

    currentZoomFactor = glm::lerp(currentZoomFactor, targetZoomFactor, glm::min(1.0F, (float)((targetZoomFactor > currentZoomFactor ? zoomInSpeed : zoomOutSpeed) * dt)));

    Camera& camera = Engine::scene()->getMainCameraEntity().getComponent<Camera>();
    double fov = glm::radians(90.0);
    camera.setFov(fov / currentZoomFactor);
}