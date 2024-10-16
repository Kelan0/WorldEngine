#include "TerrainTestApplication.h"
#include "core/graphics/ImageCube.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/Material.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/bound/BoundingVolume.h"
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/scene/terrain/QuadtreeTerrainComponent.h"
#include "core/engine/scene/terrain/TerrainTileQuadtree.h"
#include "core/engine/scene/terrain/tileSupplier/TestTerrainTileSupplier.h"
#include "core/engine/scene/terrain/tileSupplier/HeightmapTerrainTileSupplier.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Mesh.h"
#include "core/application/InputHandler.h"
#include "core/engine/renderer/LightComponent.h"
#include "core/engine/renderer/RenderLight.h"

TerrainTestApplication::TerrainTestApplication() {
}

TerrainTestApplication::~TerrainTestApplication() {
}

bool startOrtho = false;
float orthoSize = 3000.0F;

void TerrainTestApplication::init() {

    setTickrate(60.0);

    MeshData<Vertex> testMeshData;

    if (startOrtho) {
        Engine::scene()->getMainCameraEntity().getComponent<Camera>().setOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1, 35000);
        Engine::scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(5000.0F, 5000.0F, 5000.0F).setRotation(glm::vec3(-1.333F, -0.90F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
    } else {
        Engine::scene()->getMainCameraEntity().getComponent<Camera>().setClippingPlanes(0.6, 35000.0);
        Engine::scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(0.0F, 2.0F, 0.0F);
    }

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

    Entity sunLightEntity = EntityHierarchy::create(Engine::scene(), "sunLightEntity");
    sunLightEntity.addComponent<Transform>().setRotation(glm::vec3(-1.333F, -0.90F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
    glm::vec3 sunIntensity = glm::vec3(100.0F);
    sunLightEntity.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(sunIntensity).setAngularSize(glm::radians(0.52F)).setShadowCaster(false).setShadowCascadeDistances({128.0F});

//    std::string heightmapFilePath = "terrain/heightmap_1/heightmap2.hdr";
//    std::string heightmapFilePath = "terrain/UK.tif";
    std::string heightmapFilePath = "terrain/botw.png";
//    std::string heightmapFilePath = "environment_maps/rustig_koppie_puresky_8k.hdr";
    ImageData* heightmapImageData = ImageData::load(heightmapFilePath, ImagePixelLayout::RGBA, ImagePixelFormat::Float32);
//    std::shared_ptr<TerrainTileSupplier> tileSupplier = std::make_shared<HeightmapTerrainTileSupplier>(heightmapImageData);
    std::shared_ptr<TerrainTileSupplier> tileSupplier = std::make_shared<TestTerrainTileSupplier>(heightmapImageData);
    std::shared_ptr<TerrainTileSupplier> tileSupplier2 = std::make_shared<TestTerrainTileSupplier>(heightmapImageData);

    Entity terrainEntity0 = EntityHierarchy::create(Engine::scene(), "terrainEntity0");
    terrainEntity0.addComponent<Transform>().translate(2000.0, 0.0, 0.0).rotate(0, 1, 0, glm::radians(22.5F));
    QuadtreeTerrainComponent& terrain0 = terrainEntity0.addComponent<QuadtreeTerrainComponent>().setTileSupplier(tileSupplier).setSize(glm::dvec2(10000.0, 10000.0)).setHeightScale(1000.0).setMaxQuadtreeDepth(12);

//    Entity terrainEntity1 = EntityHierarchy::create(Engine::scene(), "terrainEntity1");
//    terrainEntity1.addComponent<Transform>().translate(0.0, 5000.0, -5000.0).rotate(1.0F, 0.0F, 0.0F, glm::pi<float>() * 0.5F);
//    QuadtreeTerrainComponent& terrain1 = terrainEntity1.addComponent<QuadtreeTerrainComponent>().setTileSupplier(tileSupplier2).setSize(glm::dvec2(10000.0, 10000.0)).setHeightScale(1000.0).setMaxQuadtreeDepth(12);


    testMeshData.clear();
    testMeshData.createUVSphere(glm::vec3(0.0F), 0.5F, 45, 45);
    testMeshData.computeTangents();
    MeshConfiguration sphereMeshConfig{};
    sphereMeshConfig.device = Engine::graphics()->getDevice();
    sphereMeshConfig.setMeshData(&testMeshData);
    std::shared_ptr<Mesh> sphereMesh = std::shared_ptr<Mesh>(Mesh::create(sphereMeshConfig, "Demo-SphereMesh"));

    size_t numSpheresX = 50;
    size_t numSpheresZ = 50;

    for (size_t i = 0; i < numSpheresX; ++i) {
        for (size_t j = 0; j < numSpheresZ; ++j) {

            MaterialConfiguration sphereMaterial1Config{};
            sphereMaterial1Config.device = Engine::graphics()->getDevice();
            sphereMaterial1Config.setAlbedo(glm::vec3(0.5F));
            sphereMaterial1Config.setRoughness(1.0F - (((float)i + 0.5F) / (float)numSpheresX));
            sphereMaterial1Config.setMetallic(1.0F - (((float)j + 0.5F) / (float)numSpheresX));
            std::shared_ptr<Material> sphereMaterial1 = std::shared_ptr<Material>(Material::create(sphereMaterial1Config, "Demo-SphereMaterial1-" + std::to_string(i) + "-" + std::to_string(j)));

            glm::dvec2 normPos = glm::dvec2(i, j) / glm::dvec2(numSpheresX - 1, numSpheresZ - 1);
            glm::dvec3 pos = terrain0.getTileQuadtree()->getNodePosition(normPos);

//            Entity sphereEntity1 = EntityHierarchy::create(Engine::scene(), "sphereEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");
            Entity sphereEntity1 = EntityHierarchy::createChild(terrainEntity0, "sphereEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");
            sphereEntity1.addComponent<Transform>().translate(pos.x, pos.z + 0.5F, pos.y);
            sphereEntity1.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial1);
        }
    }
}

void TerrainTestApplication::cleanup() {

}

void TerrainTestApplication::render(double dt) {
    PROFILE_SCOPE("TerrainTestApplication::render")
    handleUserInput(dt);

    Entity terrainEntity1 = Engine::scene()->findNamedEntity("terrainEntity0");
    Transform& terrainEntityTransform = terrainEntity1.getComponent<Transform>();

//    terrainEntityTransform.rotate(0, 1, 0, (float)(glm::pi<double>() * dt * 0.01F));

    Entity mainCamera = Engine::scene()->getMainCameraEntity();
    Transform& cameraTransform = mainCamera.getComponent<Transform>();
    Camera& cameraProjection = mainCamera.getComponent<Camera>();

    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_Projection);
    Engine::instance()->getImmediateRenderer()->pushMatrix("TerrainTestApplication::render/Projection");
    Engine::instance()->getImmediateRenderer()->loadMatrix(cameraProjection.getProjectionMatrix());
    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_ModelView);
    Engine::instance()->getImmediateRenderer()->pushMatrix("TerrainTestApplication::render/ModelView");
    Engine::instance()->getImmediateRenderer()->loadMatrix(glm::inverse(glm::mat4(cameraTransform.getMatrix())));

    Engine::instance()->getImmediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
    Engine::instance()->getImmediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);

    double aspect = Application::instance()->getWindowAspectRatio();

    if (input()->keyPressed(SDL_SCANCODE_F)) {
        Engine::instance()->setViewFrustumPaused(!Engine::instance()->isViewFrustumPaused());
        if (startOrtho) {
            if (!Engine::instance()->isViewFrustumPaused()) {
                cameraProjection.setOrtho(-orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, 1, 35000);
            } else {
                cameraProjection.setPerspective(90.0F, aspect, 1.0F, 35000.0F);
            }
        }
    }

    if (Engine::instance()->isViewFrustumPaused()) {
//        glm::dvec3 forward = Engine::scene()->getMainCameraEntity().getComponent<Transform>().getForwardAxis();
        glm::dvec3 forward = Engine::instance()->getViewFrustum()->getForwardAxis();
        BoundingSphere bs(Engine::instance()->getViewFrustum()->getOrigin() - forward * 5.0, 4.0);

//        Engine::instance()->getImmediateRenderer()->setColourMultiplierEnabled(true);
//        Engine::instance()->getImmediateRenderer()->setBackfaceColourMultiplier(glm::vec4(0.2F, 1.0F, 0.7F, 1.0F));

        Engine::instance()->getImmediateRenderer()->setBlendEnabled(true);
        Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(true);
        Engine::instance()->getImmediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.25F);
        Engine::instance()->getViewFrustum()->drawFill();
//        bs.drawFill();

        Engine::instance()->getImmediateRenderer()->setLineWidth(1.0F);
        Engine::instance()->getImmediateRenderer()->setBlendEnabled(false);
        Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(false);
        Engine::instance()->getImmediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
        Engine::instance()->getViewFrustum()->drawLines();
//        bs.drawLines();
    }

    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_ModelView, "TerrainTestApplication::render/ModelView");
    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_Projection, "TerrainTestApplication::render/Projection");
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

        cameraTransform.setRotation((float)cameraPitch, (float)cameraYaw);

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

            int speedIncr = input()->getMouseScrollAmount().y > 0 ? 1 : input()->getMouseScrollAmount().y < 0 ? -1 : 0;
            if (speedIncr != 0) {
                playerMovementSpeed *= glm::pow(1.15F, (float)speedIncr);
                float maxPlayerMovementSpeed = glm::pow(1.15F, 120.0F);
                float minPlayerMovementSpeed = glm::pow(1.15F, -20.0F);
                playerMovementSpeed = glm::clamp(playerMovementSpeed, minPlayerMovementSpeed, maxPlayerMovementSpeed);
            }
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
    double aspect = Application::instance()->getWindowAspectRatio();

    if (camera.isOrtho()) {
        camera.setOrtho(-orthoSize * aspect / currentZoomFactor, orthoSize * aspect / currentZoomFactor, -orthoSize / currentZoomFactor, orthoSize / currentZoomFactor, 1.0F, 35000.0F);
    } else {
        double fov = glm::radians(90.0);
        camera.setPerspective(fov / currentZoomFactor, aspect, 1.0F, 35000.0F);
//        camera.setFov(fov / currentZoomFactor);
    }
}