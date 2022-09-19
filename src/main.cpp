
#include "core/application/Application.h"
#include "core/application/Engine.h"
#include "core/application/InputHandler.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/ImageView.h"
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
#include "core/engine/renderer/LightComponent.h"

#include <iostream>

struct UniformData {
    glm::mat4 viewProjectionMatrix;
    glm::mat4 modelMatrix;
};

class App : public Application {

    std::vector<Image2D*> images;
    std::vector<Sampler*> samplers;
    std::vector<std::shared_ptr<Texture>> textures;
    double cameraPitch = 0.0F;
    double cameraYaw = 0.0F;

    std::shared_ptr<Texture> loadTexture(const std::string& filePath, vk::Format format, std::weak_ptr<Sampler> sampler) {
        Image2DConfiguration imageConfig;
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.filePath = filePath;
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled;
        imageConfig.format = format;
        imageConfig.mipLevels = 3;
        imageConfig.generateMipmap = true;
        Image2D* image = Image2D::create(imageConfig);
        images.emplace_back(image);

        ImageViewConfiguration imageViewConfig;
        imageViewConfig.device = Engine::graphics()->getDevice();
        imageViewConfig.image = image->getImage();
        imageViewConfig.format = format;
        imageViewConfig.baseMipLevel = 0;
        imageViewConfig.mipLevelCount = image->getMipLevelCount();

        return std::shared_ptr<Texture>(Texture::create(imageViewConfig, sampler));
    }

    void init() override {
        //eventDispatcher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

        SamplerConfiguration samplerConfig;
        samplerConfig.device = Engine::graphics()->getDevice();
        samplerConfig.minFilter = vk::Filter::eLinear;
        samplerConfig.magFilter = vk::Filter::eLinear;
        samplerConfig.minLod = 0.0F;
        samplerConfig.maxLod = 3.0F;
        samplerConfig.mipLodBias = 0.0F;
        std::shared_ptr<Sampler> sampler = std::shared_ptr<Sampler>(Sampler::create(samplerConfig));

        MaterialConfiguration floorMaterialConfig;
        floorMaterialConfig.setAlbedoMap(loadTexture("res/textures/blacktiles04/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
        floorMaterialConfig.setRoughnessMap(loadTexture("res/textures/blacktiles04/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        floorMaterialConfig.setMetallicMap(loadTexture("res/textures/blacktiles04/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
        floorMaterialConfig.setNormalMap(loadTexture("res/textures/blacktiles04/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
        std::shared_ptr<Material> floorMaterial = std::shared_ptr<Material>(Material::create(floorMaterialConfig));

        MaterialConfiguration cubeMaterialConfig;
        cubeMaterialConfig.setAlbedoMap(loadTexture("res/textures/mossybark02/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
        cubeMaterialConfig.setRoughnessMap(loadTexture("res/textures/mossybark02/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        cubeMaterialConfig.setMetallicMap(loadTexture("res/textures/mossybark02/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
        cubeMaterialConfig.setNormalMap(loadTexture("res/textures/mossybark02/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
        std::shared_ptr<Material> cubeMaterial = std::shared_ptr<Material>(Material::create(cubeMaterialConfig));






        MeshData<Vertex> testMeshData;
        testMeshData.createCuboid(glm::vec3(-0.5, -0.5F, -0.5), glm::vec3(+0.5, +0.5F, +0.5));
        testMeshData.computeTangents();
        MeshConfiguration cubeMeshConfig;
        cubeMeshConfig.device = Engine::graphics()->getDevice();
        cubeMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> cubeMesh = std::shared_ptr<Mesh>(Mesh::create(cubeMeshConfig));
        Entity cubeEntity = EntityHierarchy::create(Engine::scene(), "cubeEntity");
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
        floorMeshConfig.device = Engine::graphics()->getDevice();
        floorMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> floorMesh = std::shared_ptr<Mesh>(Mesh::create(floorMeshConfig));

        Entity floorEntity = EntityHierarchy::create(Engine::scene(), "floorEntity");
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
        bunnyMeshConfig.device = Engine::graphics()->getDevice();
        bunnyMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> bunnyMesh = std::shared_ptr<Mesh>(Mesh::create(bunnyMeshConfig));
//
        MaterialConfiguration bunnyMaterialConfig;
        bunnyMaterialConfig.setAlbedo(glm::vec3(0.8F, 0.7F, 0.6F));
        bunnyMaterialConfig.setRoughness(0.23F);
        std::shared_ptr<Material> bunnyMaterial = std::shared_ptr<Material>(Material::create(bunnyMaterialConfig));

        Entity bunnyEntity = EntityHierarchy::create(Engine::scene(), "bunnyEntity");
        bunnyEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
        bunnyEntity.addComponent<RenderComponent>().setMesh(bunnyMesh).setMaterial(bunnyMaterial);

        testMeshData.clear();
        testMeshData.createUVSphere(glm::vec3(0.0F), 0.25F, 45, 45);
        MeshConfiguration sphereMeshConfig;
        sphereMeshConfig.device = Engine::graphics()->getDevice();
        sphereMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> sphereMesh = std::shared_ptr<Mesh>(Mesh::create(sphereMeshConfig));

        MaterialConfiguration sphereMaterial0Config;
        sphereMaterial0Config.setAlbedo(glm::vec3(0.2F));
        sphereMaterial0Config.setMetallic(0.9F);
        sphereMaterial0Config.setRoughness(0.4F);
        std::shared_ptr<Material> sphereMaterial0 = std::shared_ptr<Material>(Material::create(sphereMaterial0Config));

        Entity sphereEntity0 = EntityHierarchy::create(Engine::scene(), "sphereEntity0");
        sphereEntity0.addComponent<Transform>().translate(-0.9, 0.333, 0.3);
        sphereEntity0.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial0);

        size_t numSpheresX = 10;
        size_t numSpheresZ = 10;

        for (size_t i = 0; i < numSpheresX; ++i) {
            for (size_t j = 0; j < numSpheresZ; ++j) {

                MaterialConfiguration sphereMaterial1Config;
                sphereMaterial1Config.setAlbedo(glm::vec3(0.5F));
                sphereMaterial1Config.setRoughness(1.0F - (((float)i + 0.5F) / (float)numSpheresX));
                sphereMaterial1Config.setMetallic(1.0F - (((float)j + 0.5F) / (float)numSpheresX));
                std::shared_ptr<Material> sphereMaterial1 = std::shared_ptr<Material> (Material::create(sphereMaterial1Config));

                Entity sphereEntity1 = EntityHierarchy::create(Engine::scene(), "sphereEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");
                sphereEntity1.addComponent<Transform>().translate(-4.0F + i * 0.26F, 0.333F, 2.0F + j * 0.26F).scale(0.5F);
                sphereEntity1.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial1);
            }
        }

        MaterialConfiguration glowMaterialConfig;
        glowMaterialConfig.setAlbedo(glm::vec3(1.0F, 1.0F, 1.0F));
        glowMaterialConfig.setRoughness(1.0F);
        glowMaterialConfig.setMetallic(0.0F);

//        Entity lightEntity1 = EntityHierarchy::create(Engine::scene(), "lightEntity1");
//        lightEntity1.addComponent<Transform>().translate(3.0, 0.8, -1.0).scale(0.125F);
//        lightEntity1.addComponent<LightComponent>().setType(LightType_Point).setIntensity(32.0, 8.0, 0.0);
//        glowMaterialConfig.setEmission(glm::vec3(32.0, 8.0, 0.0));
//        lightEntity1.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig)));
//
//        Entity lightEntity2 = EntityHierarchy::create(Engine::scene(), "lightEntity2");
//        lightEntity2.addComponent<Transform>().translate(0.4, 1.3, 2.0).scale(0.125F);
//        lightEntity2.addComponent<LightComponent>().setType(LightType_Point).setIntensity(32.0, 32.0, 32.0);
//        glowMaterialConfig.setEmission(glm::vec3(32.0, 32.0, 32.0));
//        lightEntity2.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig)));
//
//        Entity lightEntity3 = EntityHierarchy::create(Engine::scene(), "lightEntity3");
//        lightEntity3.addComponent<Transform>().translate(-2.0, 1.1, -1.2).scale(0.125F);
//        lightEntity3.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8, 6.4, 32.0);
//        glowMaterialConfig.setEmission(glm::vec3(0.8, 6.4, 32.0));
//        lightEntity3.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig)));
//
//        Entity lightEntity4 = EntityHierarchy::create(Engine::scene(), "lightEntity4");
//        lightEntity4.addComponent<Transform>().translate(-2.1, 1.1, 2.3).scale(0.125F);
//        lightEntity4.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8, 32.0, 6.4);
//        glowMaterialConfig.setEmission(glm::vec3(0.8, 32.0, 6.4));
//        lightEntity4.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig)));
//
//        Entity lightEntity5 = EntityHierarchy::create(Engine::scene(), "lightEntity5");
//        lightEntity5.addComponent<Transform>().translate(3.1, 1.1, 1.1).scale(0.125F);
//        lightEntity5.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8, 32.0, 41.0);
//        glowMaterialConfig.setEmission(glm::vec3(0.8, 32.0, 41.0));
//        lightEntity5.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig)));

        Entity lightEntity6 = EntityHierarchy::create(Engine::scene(), "lightEntity6");
        lightEntity6.addComponent<Transform>().setRotation(glm::vec3(-1.0F, -1.3F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
        lightEntity6.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(100.0, 100.0, 100.0).setShadowCaster(false);

        Engine::scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(0.0F, 1.0F, 1.0F);
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

        Entity mainCamera = Engine::scene()->getMainCameraEntity();
        Transform& cameraTransform = mainCamera.getComponent<Transform>();
        Camera& cameraProjection = mainCamera.getComponent<Camera>();


        Engine::immediateRenderer()->matrixMode(MatrixMode_Projection);
        Engine::immediateRenderer()->pushMatrix();
        Engine::immediateRenderer()->loadMatrix(cameraProjection.getProjectionMatrix());
        Engine::immediateRenderer()->matrixMode(MatrixMode_ModelView);
        Engine::immediateRenderer()->pushMatrix();
        Engine::immediateRenderer()->loadMatrix(glm::inverse(glm::mat4(cameraTransform.getMatrix())));

        Engine::immediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
        Engine::immediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);
//
//        Engine::immediateRenderer()->setBlendEnabled(true);
//        Engine::immediateRenderer()->setDepthTestEnabled(true);
//        Engine::immediateRenderer()->matrixMode(MatrixMode_ModelView);
//        Engine::immediateRenderer()->pushMatrix();
//        Engine::immediateRenderer()->translate(2.0F, 2.0F, 0.0F);
//
//        Engine::immediateRenderer()->begin(PrimitiveType_TriangleStrip);
//        Engine::immediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.5F);
//        Engine::immediateRenderer()->vertex(0.0F, 0.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(1.0F, 0.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(0.0F, 1.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(1.0F, 1.0F, 0.0F);
//        Engine::immediateRenderer()->end();
//
//        Engine::immediateRenderer()->setBlendEnabled(false);
//        Engine::immediateRenderer()->setDepthTestEnabled(false);
//        Engine::immediateRenderer()->setLineWidth(3.0F);
//        Engine::immediateRenderer()->begin(PrimitiveType_LineLoop);
//        Engine::immediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
//        Engine::immediateRenderer()->vertex(0.0F, 0.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(1.0F, 0.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(1.0F, 1.0F, 0.0F);
//        Engine::immediateRenderer()->vertex(0.0F, 1.0F, 0.0F);
//        Engine::immediateRenderer()->end();
//
//        Engine::immediateRenderer()->popMatrix();

        if (input()->keyPressed(SDL_SCANCODE_F)) {
            if (!pauseFrustum) {
                pauseFrustum = true;
                frustum.set(cameraTransform, cameraProjection);
            } else {
                pauseFrustum = false;
            }
        }

        if (pauseFrustum) {
            Engine::immediateRenderer()->setBlendEnabled(true);
            Engine::immediateRenderer()->setDepthTestEnabled(true);
            Engine::immediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.25F);
            frustum.drawFill();

            Engine::immediateRenderer()->setLineWidth(1.0F);
            Engine::immediateRenderer()->setBlendEnabled(false);
            Engine::immediateRenderer()->setDepthTestEnabled(false);
            Engine::immediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
            frustum.drawLines();
        }

        Engine::immediateRenderer()->popMatrix(MatrixMode_ModelView);
        Engine::immediateRenderer()->popMatrix(MatrixMode_Projection);
    }
};



int main(int argc, char* argv[]) {
    char* buffer = new char[1024 * 1024];
    setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

    return Application::create<App>(argc, argv);
}