
#include "core/application/Application.h"
#include "core/application/InputHandler.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/Texture.h"
#include "core/graphics/DeviceMemory.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/EntityHierarchy.h"
#include "core/engine/scene/Transform.h"
#include "core/engine/scene/Camera.h"
#include "core/engine/scene/event/EventDispacher.h"
#include "core/engine/scene/event/Events.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/Profiler.h"

#include <iostream>

struct UniformData {
    glm::mat4 viewProjectionMatrix;
    glm::mat4 modelMatrix;
};

class App : public Application {

    std::vector<Image2D*> images;
    std::vector<std::shared_ptr<Texture2D>> textures;
    RenderCamera camera;
    double cameraPitch = 0.0F;
    double cameraYaw = 0.0F;

    std::vector<Entity> testEntities;
    std::vector<Entity> dynamicEntities;

    void init() override {
        //eventDispacher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);


        GraphicsPipelineConfiguration pipelineConfig;
        pipelineConfig.vertexShader = "res/shaders/main.vert";
        pipelineConfig.fragmentShader = "res/shaders/main.frag";
        pipelineConfig.vertexInputBindings = Mesh::getVertexBindingDescriptions();
        pipelineConfig.vertexInputAttributes = Mesh::getVertexAttributeDescriptions();
        renderer()->initPipelineDescriptorSetLayouts(pipelineConfig);
        graphics()->configurePipeline(pipelineConfig);



        Image2DConfiguration testTextureImageConfig;
        testTextureImageConfig.device = graphics()->getDevice();
        testTextureImageConfig.filePath = "res/textures/Brick_Wall_017_SD/Brick_Wall_017_basecolor.jpg";
        testTextureImageConfig.usage = vk::ImageUsageFlagBits::eSampled;
        testTextureImageConfig.format = vk::Format::eR8G8B8A8Srgb;
        Image2D* testImage = Image2D::create(testTextureImageConfig);
        images.emplace_back(testImage);

        SamplerConfiguration testTextureSamplerConfig;
        testTextureSamplerConfig.device = graphics()->getDevice();
        testTextureSamplerConfig.minFilter = vk::Filter::eLinear;
        testTextureSamplerConfig.magFilter = vk::Filter::eLinear;
        ImageView2DConfiguration testTextureImageViewConfig;
        testTextureImageViewConfig.device = graphics()->getDevice();
        testTextureImageViewConfig.image = testImage->getImage();
        testTextureImageViewConfig.format = testImage->getFormat();
        std::shared_ptr<Texture2D> testTexture = std::shared_ptr<Texture2D>(Texture2D::create(testTextureImageViewConfig, testTextureSamplerConfig));

        MeshData testMeshData;
        testMeshData.createCuboid(glm::vec3(-0.5, 2, -0.5), glm::vec3(+0.5, 3, +0.5));
        testMeshData.scale(10.0);
        testMeshData.createQuad(glm::vec3(-1.0F, 0.0F, -1.0F), glm::vec3(-1.0F, 0.0F, +1.0F), glm::vec3(+1.0F, 0.0F, +1.0F), glm::vec3(+1.0F, 0.0F, -1.0F), glm::vec3(0, 1, 0));
        MeshConfiguration floorMeshConfig;
        floorMeshConfig.device = graphics()->getDevice();
        floorMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> floorMesh = std::shared_ptr<Mesh>(Mesh::create(floorMeshConfig));

        Entity floorEntity = EntityHierarchy::create(scene(), "floorEntity");
        floorEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
        floorEntity.addComponent<RenderComponent>().setMesh(floorMesh).setTexture(testTexture);




        testMeshData = MeshData();
        testMeshData.scale(0.5);
        MeshLoader::loadOBJ("res/meshes/bunny.obj", testMeshData);
        glm::vec3 centerBottom = testMeshData.calculateBoundingBox() * glm::vec4(0, -1, 0, 1);
        testMeshData.translate(-1.0F * centerBottom);
        testMeshData.applyTransform();

        printf("Loaded bunny.obj :- %llu polygons\n", testMeshData.getPolygonCount());



        MeshConfiguration testMeshConfig;
        testMeshConfig.device = graphics()->getDevice();
        testMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> testMesh = std::shared_ptr<Mesh>(Mesh::create(testMeshConfig));

        const int xCount = 1000;
        const int zCount = 1000;
        const double spacing = 1.1F;

        glm::u8vec4 pixel;
        pixel.a = 0xFF;

        testMeshData = MeshData();
        testMeshData.createCuboid(glm::vec3(-0.5, -0.5F, -0.5), glm::vec3(+0.5, +0.5F, +0.5));

        MeshConfiguration cubeMeshConfig;
        cubeMeshConfig.device = graphics()->getDevice();
        cubeMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> cubeMesh = std::shared_ptr<Mesh>(Mesh::create(cubeMeshConfig));

        for (int i = 0; i < 10000; ++i) {
            pixel.r = rand() % 0xFF; //(uint8_t)((double)i / (double)xCount * 0xFF);
            pixel.g = rand() % 0xFF; //(uint8_t)((double)j / (double)zCount * 0xFF);
            pixel.b = rand() % 0xFF;

            ImageData imageData(&pixel[0], 1, 1, ImagePixelLayout::RGBA, ImagePixelFormat::UInt8);

            Image2DConfiguration bunnyTextureImageConfig;
            bunnyTextureImageConfig.device = graphics()->getDevice();
            bunnyTextureImageConfig.imageData = &imageData;
            bunnyTextureImageConfig.usage = vk::ImageUsageFlagBits::eSampled;
            bunnyTextureImageConfig.format = vk::Format::eR8G8B8A8Srgb;
            Image2D* bunnyImage = Image2D::create(bunnyTextureImageConfig);
            images.emplace_back(bunnyImage);

            SamplerConfiguration bunnyTextureSamplerConfig;
            bunnyTextureSamplerConfig.device = graphics()->getDevice();
            bunnyTextureSamplerConfig.minFilter = vk::Filter::eNearest;
            bunnyTextureSamplerConfig.magFilter = vk::Filter::eNearest;
            ImageView2DConfiguration bunnyTextureImageViewConfig;
            bunnyTextureImageViewConfig.device = graphics()->getDevice();
            bunnyTextureImageViewConfig.image = bunnyImage->getImage();
            bunnyTextureImageViewConfig.format = bunnyImage->getFormat();
            std::shared_ptr<Texture2D> texture = std::shared_ptr<Texture2D>(Texture2D::create(bunnyTextureImageViewConfig, bunnyTextureSamplerConfig));

            textures.emplace_back(texture);

        }


        for (int i = 0; i < xCount; ++i) {
            for (int j = 0; j < zCount; ++j) {

                Entity entity = EntityHierarchy::create(scene(), "testEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");

                Transform& t = entity.addComponent<Transform>()
                    .translate(-0.5 * spacing * xCount + i * spacing, 0.0, -0.5 * spacing * zCount + j * spacing)
                    .scale(0.75);

                entity.addComponent<RenderComponent>()
                    .setMesh(cubeMesh)
                    .setTexture(textures[rand() % textures.size()]);

                testEntities.push_back(entity);
                double distSq = glm::dot(t.getTranslation(), t.getTranslation());
                if (distSq < 10 * 10) {
                  t.translate(0, 1, 0);
                  dynamicEntities.push_back(entity);
                }
            }
        }



        //Entity testEntity = EntityHierarchy::create(scene(), "testEntity");
        //testEntity.addComponent<Transform>().translate(1.0, 0.0, -2.0);
        //testEntity.addComponent<RenderComponent>().setMesh(testMesh).setTexture(testTexture);
        //
        //Entity testEntity2 = EntityHierarchy::create(scene(), "testEntity2");
        //testEntity2.addComponent<Transform>().translate(0.0, 0.4, -2.0);
        //testEntity2.addComponent<RenderComponent>().setMesh(testMesh).setTexture(testTexture);


        scene()->getMainCamera().getComponent<Transform>().setTranslation(0.0F, 2.0F, 2.0F);
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
            Transform& cameraTransform = scene()->getMainCamera().getComponent<Transform>();
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

    void render(double dt) override {
        PROFILE_SCOPE("custom render")
        handleUserInput(dt);

        PROFILE_REGION("Change transform test")
        for (Entity& ent : dynamicEntities) {
            ent.getComponent<Transform>().rotate(0, 1, 0, glm::radians(dt * 90.0));
        }

//        PROFILE_REGION("Random texture test")
//
//        size_t f = 100;
//        for (size_t i = 0; i < testEntities.size() / f; ++i) {
//        	size_t idx = std::min((i * f) + (rand() % f), testEntities.size());
//
//        	Entity& entity = testEntities[idx];
//        	entity.getComponent<RenderComponent>().setTexture(textures[rand() % textures.size()]);
//        }



        //for (int i = 0; i < testEntities.size(); ++i) {
        //	Transform& t = testEntities[i].getComponent<Transform>();
        //	t.rotate(0.0, 1.0, 0.0, dt * glm::radians(360.0F));
        //}
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