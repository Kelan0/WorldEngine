#include "TerrainTestApplication.h"
#include "core/graphics/ImageCube.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/scene/terrain/TerrainComponent.h"
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
    terrainEntity.addComponent<TerrainComponent>();
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

    if (input()->isMouseGrabbed()) {
        Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();
        glm::ivec2 dMouse = input()->getRelativeMouseState();
        if (isViewportInverted())
            dMouse.y *= -1;

        cameraPitch += dMouse.y * 0.001F;
        cameraYaw -= dMouse.x * 0.001F;
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
    }
}