
#include "core/application/Application.h"
#include "core/application/InputHandler.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/ImageCube.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/Texture.h"
#include "core/graphics/DeviceMemory.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/scene/event/EventDispatcher.h"
#include "core/engine/scene/event/Events.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/Material.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/Profiler.h"
#include "core/engine/scene/bound/Intersection.h"

#include <iostream>

struct UniformData {
    glm::mat4 viewProjectionMatrix;
    glm::mat4 modelMatrix;
};

class App : public Application {

    std::vector<Image2D*> images;
    std::vector<Sampler*> samplers;
    std::vector<std::shared_ptr<Texture2D>> textures;
    double cameraPitch = 0.0F;
    double cameraYaw = 0.0F;

    std::shared_ptr<Texture2D> loadTexture(const std::string& filePath, vk::Format format, std::weak_ptr<Sampler> sampler) {
        Image2DConfiguration imageConfig;
        imageConfig.device = graphics()->getDevice();
        imageConfig.filePath = filePath;
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled;
        imageConfig.format = format;
        Image2D* image = Image2D::create(imageConfig);
        images.emplace_back(image);

        ImageView2DConfiguration imageViewConfig;
        imageViewConfig.device = graphics()->getDevice();
        imageViewConfig.image = image->getImage();
        imageViewConfig.format = format;

        return std::shared_ptr<Texture2D>(Texture2D::create(imageViewConfig, sampler));
    }

    void init() override {
        //eventDispatcher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

        SamplerConfiguration samplerConfig;
        samplerConfig.device = graphics()->getDevice();
        samplerConfig.minFilter = vk::Filter::eLinear;
        samplerConfig.magFilter = vk::Filter::eLinear;
        std::shared_ptr<Sampler> sampler = std::shared_ptr<Sampler>(Sampler::create(samplerConfig));

        MaterialConfiguration floorMaterialConfig;
        floorMaterialConfig.setAlbedoMap(loadTexture("res/textures/blacktiles04/albedo.png", vk::Format::eR8G8B8A8Srgb, sampler));
        floorMaterialConfig.setRoughnessMap(loadTexture("res/textures/blacktiles04/roughness.png", vk::Format::eR8G8B8A8Srgb, sampler));
//        floorMaterialConfig.setMetallicMap(loadTexture("res/textures/blacktiles04/metallic.png", vk::Format::eR8G8B8A8Srgb, sampler));
        floorMaterialConfig.setNormalMap(loadTexture("res/textures/blacktiles04/normal.png", vk::Format::eR8G8B8A8Srgb, sampler));
        std::shared_ptr<Material> floorMaterial = std::shared_ptr<Material>(Material::create(floorMaterialConfig));

        MaterialConfiguration cubeMaterialConfig;
        cubeMaterialConfig.setAlbedoMap(loadTexture("res/textures/mossybark02/albedo.png", vk::Format::eR8G8B8A8Srgb, sampler));
        cubeMaterialConfig.setRoughnessMap(loadTexture("res/textures/mossybark02/roughness.png", vk::Format::eR8G8B8A8Srgb, sampler));
//        cubeMaterialConfig.setMetallicMap(loadTexture("res/textures/mossybark02/metallic.png", vk::Format::eR8G8B8A8Srgb, sampler));
        cubeMaterialConfig.setNormalMap(loadTexture("res/textures/mossybark02/normal.png", vk::Format::eR8G8B8A8Srgb, sampler));
        std::shared_ptr<Material> cubeMaterial = std::shared_ptr<Material>(Material::create(cubeMaterialConfig));






        MeshData<Vertex> testMeshData;
        testMeshData.createCuboid(glm::vec3(-0.5, -0.5F, -0.5), glm::vec3(+0.5, +0.5F, +0.5));
        testMeshData.computeTangents();
        MeshConfiguration cubeMeshConfig;
        cubeMeshConfig.device = graphics()->getDevice();
        cubeMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> cubeMesh = std::shared_ptr<Mesh>(Mesh::create(cubeMeshConfig));
        Entity cubeEntity = EntityHierarchy::create(scene(), "cubeEntity");
        cubeEntity.addComponent<Transform>().translate(1.6F, 0.5F, 0);
        cubeEntity.addComponent<RenderComponent>().setMesh(cubeMesh).setMaterial(cubeMaterial);

        testMeshData.clear();
        auto i0 = testMeshData.addVertex(-10.0F, 0.0F, -10.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F);
        auto i1 = testMeshData.addVertex(-10.0F, 0.0F, +10.0F, 0.0F, 1.0F, 0.0F, 0.0F, 4.0F);
        auto i2 = testMeshData.addVertex(+10.0F, 0.0F, +10.0F, 0.0F, 1.0F, 0.0F, 4.0F, 4.0F);
        auto i3 = testMeshData.addVertex(+10.0F, 0.0F, -10.0F, 0.0F, 1.0F, 0.0F, 4.0F, 0.0F);
        testMeshData.addQuad(i0, i1, i2, i3);
        testMeshData.computeTangents();
        MeshConfiguration floorMeshConfig;
        floorMeshConfig.device = graphics()->getDevice();
        floorMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> floorMesh = std::shared_ptr<Mesh>(Mesh::create(floorMeshConfig));

        Entity floorEntity = EntityHierarchy::create(scene(), "floorEntity");
        floorEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
        floorEntity.addComponent<RenderComponent>().setMesh(floorMesh).setMaterial(floorMaterial);


        testMeshData.clear();
        testMeshData.scale(0.5);
        MeshUtils::loadMeshData("res/meshes/bunny.obj", testMeshData);
        glm::vec3 centerBottom = testMeshData.calculateBoundingBox() * glm::vec4(0, -1, 0, 1);
        testMeshData.translate(-1.0F * centerBottom);
        testMeshData.applyTransform();
        testMeshData.computeTangents();
//
        printf("Loaded bunny.obj :- %llu polygons\n", testMeshData.getPolygonCount());
        MeshConfiguration bunnyMeshConfig;
        bunnyMeshConfig.device = graphics()->getDevice();
        bunnyMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> bunnyMesh = std::shared_ptr<Mesh>(Mesh::create(bunnyMeshConfig));
//
        MaterialConfiguration bunnyMaterialConfig;
        bunnyMaterialConfig.setAlbedo(glm::vec3(0.8F, 0.7F, 0.6F));
        bunnyMaterialConfig.setRoughness(0.23F);
        std::shared_ptr<Material> bunnyMaterial = std::shared_ptr<Material>(Material::create(bunnyMaterialConfig));

        Entity bunnyEntity = EntityHierarchy::create(scene(), "bunnyEntity");
        bunnyEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
        bunnyEntity.addComponent<RenderComponent>().setMesh(bunnyMesh).setMaterial(bunnyMaterial);

        testMeshData.clear();
        testMeshData.createUVSphere(glm::vec3(0.0F), 0.25F, 45, 45);
        MeshConfiguration sphereMeshConfig;
        sphereMeshConfig.device = graphics()->getDevice();
        sphereMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> sphereMesh = std::shared_ptr<Mesh>(Mesh::create(sphereMeshConfig));

        MaterialConfiguration sphereMaterial0Config;
        sphereMaterial0Config.setAlbedo(glm::vec3(0.2F));
        sphereMaterial0Config.setMetallic(0.9F);
        sphereMaterial0Config.setRoughness(0.4F);
        std::shared_ptr<Material> sphereMaterial0 = std::shared_ptr<Material>(Material::create(sphereMaterial0Config));

        Entity sphereEntity0 = EntityHierarchy::create(scene(), "sphereEntity0");
        sphereEntity0.addComponent<Transform>().translate(-0.9, 0.333, 0.3);
        sphereEntity0.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial0);


        scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(0.0F, 1.0F, 1.0F);
    }

    void cleanup() override {

        textures.clear();
        for (int i = 0; i < images.size(); ++i)
            delete images[i];
    }

    void handleUserInput(double dt) {
        if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
            input()->toggleMouseGrabbed();
        }

        if (input()->isMouseGrabbed()) {
            Transform& cameraTransform = Application::instance()->scene()->getMainCameraEntity().getComponent<Transform>();
            glm::ivec2 dMouse = input()->getRelativeMouseState();
            if (isViewportInverted())
                dMouse.y *= -1;

            cameraPitch += dMouse.y * 0.001F;
            cameraYaw -= dMouse.x * 0.001F;
            if (cameraYaw > +M_PI) cameraYaw -= M_PI * 2.0;
            if (cameraYaw < -M_PI) cameraYaw += M_PI * 2.0;
            if (cameraPitch > +M_PI * 0.5) cameraPitch = +M_PI * 0.5;
            if (cameraPitch < -M_PI * 0.5) cameraPitch = -M_PI * 0.5;

            cameraTransform.setRotation(cameraPitch, cameraYaw);

            double movementSpeed = 1.0;
            glm::dvec3 movementDir(0.0);

            if (input()->keyDown(SDL_SCANCODE_W)) movementDir.z--;
            if (input()->keyDown(SDL_SCANCODE_S)) movementDir.z++;
            if (input()->keyDown(SDL_SCANCODE_A)) movementDir.x--;
            if (input()->keyDown(SDL_SCANCODE_D)) movementDir.x++;

            if (glm::dot(movementDir, movementDir) > 0.5F) {
                movementDir = cameraTransform.getRotationMatrix() * glm::normalize(movementDir);
                cameraTransform.translate(movementDir * movementSpeed * dt);
            }
        }
    }

    Frustum frustum;
    bool pauseFrustum = false;

    void render(double dt) override {
        PROFILE_SCOPE("custom render")
        handleUserInput(dt);

        Entity mainCamera = Application::instance()->scene()->getMainCameraEntity();
        Transform& cameraTransform = mainCamera.getComponent<Transform>();
        Camera& cameraProjection = mainCamera.getComponent<Camera>();


        immediateRenderer()->matrixMode(MatrixMode_Projection);
        immediateRenderer()->pushMatrix();
        immediateRenderer()->loadMatrix(cameraProjection.getProjectionMatrix());
        immediateRenderer()->matrixMode(MatrixMode_ModelView);
        immediateRenderer()->pushMatrix();
        immediateRenderer()->loadMatrix(glm::inverse(glm::mat4(cameraTransform.getMatrix())));

        immediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
        immediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);
//
//        immediateRenderer()->setBlendEnabled(true);
//        immediateRenderer()->setDepthTestEnabled(true);
//        immediateRenderer()->matrixMode(MatrixMode_ModelView);
//        immediateRenderer()->pushMatrix();
//        immediateRenderer()->translate(2.0F, 2.0F, 0.0F);
//
//        immediateRenderer()->begin(PrimitiveType_TriangleStrip);
//        immediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.5F);
//        immediateRenderer()->vertex(0.0F, 0.0F, 0.0F);
//        immediateRenderer()->vertex(1.0F, 0.0F, 0.0F);
//        immediateRenderer()->vertex(0.0F, 1.0F, 0.0F);
//        immediateRenderer()->vertex(1.0F, 1.0F, 0.0F);
//        immediateRenderer()->end();
//
//        immediateRenderer()->setBlendEnabled(false);
//        immediateRenderer()->setDepthTestEnabled(false);
//        immediateRenderer()->setLineWidth(3.0F);
//        immediateRenderer()->begin(PrimitiveType_LineLoop);
//        immediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
//        immediateRenderer()->vertex(0.0F, 0.0F, 0.0F);
//        immediateRenderer()->vertex(1.0F, 0.0F, 0.0F);
//        immediateRenderer()->vertex(1.0F, 1.0F, 0.0F);
//        immediateRenderer()->vertex(0.0F, 1.0F, 0.0F);
//        immediateRenderer()->end();
//
//        immediateRenderer()->popMatrix();

        if (input()->keyPressed(SDL_SCANCODE_F)) {
            if (!pauseFrustum) {
                pauseFrustum = true;
                frustum.set(cameraTransform, cameraProjection);
            } else {
                pauseFrustum = false;
            }
        }

        if (pauseFrustum) {
            immediateRenderer()->setBlendEnabled(true);
            immediateRenderer()->setDepthTestEnabled(true);
            immediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.25F);
            frustum.drawFill();

            immediateRenderer()->setLineWidth(1.0F);
            immediateRenderer()->setBlendEnabled(false);
            immediateRenderer()->setDepthTestEnabled(false);
            immediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
            frustum.drawLines();
        }

        immediateRenderer()->popMatrix(MatrixMode_ModelView);
        immediateRenderer()->popMatrix(MatrixMode_Projection);
    }
};



int main(int argc, char* argv[]) {
    char* buffer = new char[1024 * 1024];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    return Application::create<App>();

    std::cout << std::endl;
    fflush(stdout);
    return 99;
}