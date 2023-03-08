#include "RenderStressTestApplication.h"
#include "core/engine/renderer/Material.h"
#include "core/engine/scene/Entity.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/graphics/Mesh.h"
#include "core/engine/renderer/LightComponent.h"
#include "core/application/InputHandler.h"
#include <random>

RenderStressTestApplication::RenderStressTestApplication():
    m_cameraPitch(0.0),
    m_cameraYaw(0.0),
    m_playerMovementSpeed(5.0) {
}

RenderStressTestApplication::~RenderStressTestApplication() {
}

void RenderStressTestApplication::init() {

    MeshData<Vertex> testMeshData;
    testMeshData.clear();
    testMeshData.createUVSphere(glm::vec3(0.0F), 0.25F, 6, 6);
    testMeshData.computeTangents();

    MeshConfiguration sphereMeshConfig{};
    sphereMeshConfig.device = Engine::graphics()->getDevice();
    sphereMeshConfig.setMeshData(&testMeshData);
    m_sphereMesh = std::shared_ptr<Mesh>(Mesh::create(sphereMeshConfig, "Demo-SphereMesh"));

    MaterialConfiguration sphereMaterial1Config{};
    sphereMaterial1Config.device = Engine::graphics()->getDevice();
    sphereMaterial1Config.setAlbedo(glm::vec3(0.5F));
    sphereMaterial1Config.setRoughness(0.2F);
    sphereMaterial1Config.setMetallic(0.9F);
    m_sphereMaterial = std::shared_ptr<Material> (Material::create(sphereMaterial1Config, "Demo-AddSphereMaterial"));

    BoundingVolume* sphereBounds = new BoundingSphere(glm::dvec3(0.0), 0.26);

    size_t numSpheresX = 100;
    size_t numSpheresZ = 100;
    float separationX = 0.26F;
    float separationZ = 0.26F;
    float offsetX = 0.0F;//-0.5F * (numSpheresX * separationX);
    float offsetZ = 0.0F;//-0.5F * (numSpheresZ * separationZ);

    std::random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_real_distribution<float> randf(0.0F, 1.0F);

    for (size_t i = 0; i < numSpheresX; ++i) {
        for (size_t j = 0; j < numSpheresZ; ++j) {

            sphereMaterial1Config.setAlbedo(glm::vec3(randf(gen), randf(gen), randf(gen)));
            sphereMaterial1Config.setRoughness(randf(gen));
            sphereMaterial1Config.setMetallic(randf(gen));
            std::shared_ptr<Material> sphereMaterial1 = std::shared_ptr<Material> (Material::create(sphereMaterial1Config, "Demo-SphereMaterial1-" + std::to_string(i) + "-" + std::to_string(j)));

            Entity sphereEntity1 = EntityHierarchy::create(Engine::scene(), "sphereEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");
            sphereEntity1.addComponent<Transform>().translate(offsetX + i * separationX, 0.333F, offsetZ + j * separationX).scale(0.5F);
            sphereEntity1.addComponent<RenderComponent>(RenderComponent::UpdateType_Static, RenderComponent::UpdateType_Static).setMesh(m_sphereMesh).setMaterial(sphereMaterial1);
        }
    }

    Entity sunLightEntity = EntityHierarchy::create(Engine::scene(), "sunLightEntity");
    sunLightEntity.addComponent<Transform>().setRotation(glm::vec3(-1.0F, -1.3F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
    glm::vec3 sunIntensity = glm::vec3(90.0F);
    sunLightEntity.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(sunIntensity).setAngularSize(glm::radians(0.52F)).setShadowCaster(true).setShadowCascadeDistances({3.0F, 6.0F, 12.0F, 24.0F});

    m_cameraPitch = 0.0F;
    m_cameraYaw = glm::radians(225.0F); // 45 degrees (between +X and +Z)
    Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();
    cameraTransform.setTranslation(0.0F, 1.0F, 0.0F).setRotation((float)m_cameraPitch, (float)m_cameraYaw);
}

void RenderStressTestApplication::cleanup() {
    m_sphereMesh.reset();
    m_sphereMaterial.reset();
}

uint32_t sphereCount = 0;
void RenderStressTestApplication::render(double dt) {
    PROFILE_SCOPE("custom render")
    handleUserInput(dt);

    if (input()->mousePressed(SDL_BUTTON_LEFT)) {
        ++sphereCount;

        Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();

        Entity sphereEntity = EntityHierarchy::create(Engine::scene(), "AddSphereEntity-" + std::to_string(sphereCount));
        sphereEntity.addComponent<Transform>().translate(cameraTransform.getTranslation() + glm::dvec3(cameraTransform.getForwardAxis() * 3.0F)).scale(0.5F);
        sphereEntity.addComponent<RenderComponent>().setMesh(m_sphereMesh).setMaterial(m_sphereMaterial);
    }
}

void RenderStressTestApplication::tick(double dt) {

}

void RenderStressTestApplication::handleUserInput(double dt) {
    if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
        input()->toggleMouseGrabbed();
    }

    if (input()->isMouseGrabbed()) {
        Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();
        glm::ivec2 dMouse = input()->getRelativeMouseState();
        if (isViewportInverted())
            dMouse.y *= -1;

        m_cameraPitch += dMouse.y * 0.001F;
        m_cameraYaw -= dMouse.x * 0.001F;
        if (m_cameraYaw > +M_PI) m_cameraYaw -= M_PI * 2.0;
        if (m_cameraYaw < -M_PI) m_cameraYaw += M_PI * 2.0;
        if (m_cameraPitch > +M_PI * 0.5) m_cameraPitch = +M_PI * 0.5;
        if (m_cameraPitch < -M_PI * 0.5) m_cameraPitch = -M_PI * 0.5;

        cameraTransform.setRotation((float) m_cameraPitch, (float) m_cameraYaw);

        glm::dvec3 movementDir(0.0);

        if (input()->keyDown(SDL_SCANCODE_W)) movementDir.z--;
        if (input()->keyDown(SDL_SCANCODE_S)) movementDir.z++;
        if (input()->keyDown(SDL_SCANCODE_A)) movementDir.x--;
        if (input()->keyDown(SDL_SCANCODE_D)) movementDir.x++;
        if (input()->keyDown(SDL_SCANCODE_LSHIFT)) movementDir.y--;
        if (input()->keyDown(SDL_SCANCODE_SPACE)) movementDir.y++;

        if (glm::dot(movementDir, movementDir) > 0.5F) {
            movementDir = cameraTransform.getRotationMatrix() * glm::normalize(movementDir);
            cameraTransform.translate(movementDir * (double)m_playerMovementSpeed * dt);
        }
    }
}