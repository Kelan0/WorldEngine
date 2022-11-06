
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
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/ApplicationEvents.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/ImmediateRenderer.h"
#include "core/engine/renderer/Material.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/Profiler.h"
#include "core/engine/scene/bound/Intersection.h"
#include "core/engine/renderer/LightComponent.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/renderPasses/ExposureHistogram.h"
#include "extern/imgui/imgui.h"

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
    float playerMovementSpeed = 1.0F;
    bool cameraMouseInputLocked = false;

    std::shared_ptr<Texture> loadTexture(const std::string& filePath, vk::Format format, std::weak_ptr<Sampler> sampler) {
        std::string imageName = std::string("TestImage:") + filePath;
        Image2DConfiguration imageConfig{};
        imageConfig.device = Engine::graphics()->getDevice();
        imageConfig.filePath = filePath;
        imageConfig.usage = vk::ImageUsageFlagBits::eSampled;
        imageConfig.format = format;
        imageConfig.mipLevels = 3;
        imageConfig.generateMipmap = true;
        Image2D* image = Image2D::create(imageConfig, imageName.c_str());
        images.emplace_back(image);

        std::string imageViewName = std::string("TestImageView:") + filePath;
        ImageViewConfiguration imageViewConfig{};
        imageViewConfig.device = Engine::graphics()->getDevice();
        imageViewConfig.image = image->getImage();
        imageViewConfig.format = format;
        imageViewConfig.baseMipLevel = 0;
        imageViewConfig.mipLevelCount = image->getMipLevelCount();

        return std::shared_ptr<Texture>(Texture::create(imageViewConfig, sampler, imageViewName));
    }

    void init() override {
        //eventDispatcher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

        SamplerConfiguration samplerConfig{};
        samplerConfig.device = Engine::graphics()->getDevice();
        samplerConfig.minFilter = vk::Filter::eLinear;
        samplerConfig.magFilter = vk::Filter::eLinear;
        samplerConfig.minLod = 0.0F;
        samplerConfig.maxLod = 3.0F;
        samplerConfig.mipLodBias = 0.0F;
        std::shared_ptr<Sampler> sampler = std::shared_ptr<Sampler>(Sampler::create(samplerConfig, "DemoMaterialSampler"));

        MaterialConfiguration floorMaterialConfig{};
        floorMaterialConfig.device = Engine::graphics()->getDevice();
        floorMaterialConfig.setAlbedoMap(loadTexture("res/textures/blacktiles04/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
        floorMaterialConfig.setRoughnessMap(loadTexture("res/textures/blacktiles04/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        floorMaterialConfig.setMetallicMap(loadTexture("res/textures/blacktiles04/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
        floorMaterialConfig.setNormalMap(loadTexture("res/textures/blacktiles04/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
        std::shared_ptr<Material> floorMaterial = std::shared_ptr<Material>(Material::create(floorMaterialConfig, "Demo-FloorMaterial"));

        MaterialConfiguration cubeMaterialConfig{};
        cubeMaterialConfig.device = Engine::graphics()->getDevice();
        cubeMaterialConfig.setAlbedoMap(loadTexture("res/textures/mossybark02/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
        cubeMaterialConfig.setRoughnessMap(loadTexture("res/textures/mossybark02/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        cubeMaterialConfig.setMetallicMap(loadTexture("res/textures/mossybark02/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
        cubeMaterialConfig.setNormalMap(loadTexture("res/textures/mossybark02/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
        std::shared_ptr<Material> cubeMaterial = std::shared_ptr<Material>(Material::create(cubeMaterialConfig, "Demo-CubeMaterial"));

        MeshData<Vertex> testMeshData;
        testMeshData.createCuboid(glm::vec3(-0.5, -0.5F, -0.5), glm::vec3(+0.5, +0.5F, +0.5));
        testMeshData.computeTangents();
        MeshConfiguration cubeMeshConfig{};
        cubeMeshConfig.device = Engine::graphics()->getDevice();
        cubeMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> cubeMesh = std::shared_ptr<Mesh>(Mesh::create(cubeMeshConfig, "Demo-CubeMesh"));
        Entity cubeEntity = EntityHierarchy::create(Engine::scene(), "cubeEntity");
        cubeEntity.addComponent<Transform>().translate(1.5F, 0.5F, 0);
        cubeEntity.addComponent<RenderComponent>().setMesh(cubeMesh).setMaterial(cubeMaterial);

        testMeshData.clear();
        float floorSize = 12.0F;
        auto i0 = testMeshData.addVertex(-floorSize, 0.0F, -floorSize, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F);
        auto i1 = testMeshData.addVertex(-floorSize, 0.0F, +floorSize, 0.0F, 1.0F, 0.0F, 0.0F, 4.0F);
        auto i2 = testMeshData.addVertex(+floorSize, 0.0F, +floorSize, 0.0F, 1.0F, 0.0F, 4.0F, 4.0F);
        auto i3 = testMeshData.addVertex(+floorSize, 0.0F, -floorSize, 0.0F, 1.0F, 0.0F, 4.0F, 0.0F);
        testMeshData.addQuad(i0, i1, i2, i3);
        testMeshData.computeTangents();
        MeshConfiguration floorMeshConfig{};
        floorMeshConfig.device = Engine::graphics()->getDevice();
        floorMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> floorMesh = std::shared_ptr<Mesh>(Mesh::create(floorMeshConfig, "Demo-FloorMesh"));

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
        MeshConfiguration bunnyMeshConfig{};
        bunnyMeshConfig.device = Engine::graphics()->getDevice();
        bunnyMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> bunnyMesh = std::shared_ptr<Mesh>(Mesh::create(bunnyMeshConfig, "Demo-BunnyMesh"));
//
        MaterialConfiguration bunnyMaterialConfig{};
        bunnyMaterialConfig.device = Engine::graphics()->getDevice();
        bunnyMaterialConfig.setAlbedo(glm::vec3(0.8F, 0.7F, 0.6F));
        bunnyMaterialConfig.setRoughness(0.23F);
        std::shared_ptr<Material> bunnyMaterial = std::shared_ptr<Material>(Material::create(bunnyMaterialConfig, "Demo-BunnyMaterial"));

        Entity bunnyEntity = EntityHierarchy::create(Engine::scene(), "bunnyEntity");
        bunnyEntity.addComponent<Transform>().translate(0.0, 0.0, 0.0);
        bunnyEntity.addComponent<RenderComponent>().setMesh(bunnyMesh).setMaterial(bunnyMaterial);

        testMeshData.clear();
        testMeshData.createUVSphere(glm::vec3(0.0F), 0.25F, 45, 45);
        testMeshData.computeTangents();
        MeshConfiguration sphereMeshConfig{};
        sphereMeshConfig.device = Engine::graphics()->getDevice();
        sphereMeshConfig.setMeshData(&testMeshData);
        std::shared_ptr<Mesh> sphereMesh = std::shared_ptr<Mesh>(Mesh::create(sphereMeshConfig, "Demo-SphereMesh"));

        MaterialConfiguration sphereMaterial0Config{};
        sphereMaterial0Config.device = Engine::graphics()->getDevice();
        sphereMaterial0Config.setAlbedo(glm::vec3(0.2F));
        sphereMaterial0Config.setMetallic(0.9F);
        sphereMaterial0Config.setRoughness(0.4F);
        std::shared_ptr<Material> sphereMaterial0 = std::shared_ptr<Material>(Material::create(sphereMaterial0Config, "Demo-SphereMaterial0"));

        Entity sphereEntity0 = EntityHierarchy::create(Engine::scene(), "sphereEntity0");
        sphereEntity0.addComponent<Transform>().translate(-0.9, 0.333, 0.3);
        sphereEntity0.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial0);

        MaterialConfiguration christmasBallMaterialConfig{};
        christmasBallMaterialConfig.device = Engine::graphics()->getDevice();
        christmasBallMaterialConfig.setAlbedoMap(loadTexture("res/textures/christmas_tree_ball/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
        christmasBallMaterialConfig.setRoughnessMap(loadTexture("res/textures/christmas_tree_ball/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
        christmasBallMaterialConfig.setMetallicMap(loadTexture("res/textures/christmas_tree_ball/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
        christmasBallMaterialConfig.setNormalMap(loadTexture("res/textures/christmas_tree_ball/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
        christmasBallMaterialConfig.setDisplacementMap(loadTexture("res/textures/christmas_tree_ball/displacement.png", vk::Format::eR8G8B8A8Unorm, sampler));
        std::shared_ptr<Material> christmasBallMaterial = std::shared_ptr<Material>(Material::create(christmasBallMaterialConfig, "Demo-ChristmasBallMaterial"));

        Entity christmasBallEntity = EntityHierarchy::create(Engine::scene(), "christmasBallEntity");
        christmasBallEntity.addComponent<Transform>().translate(-2.0, 0.6, 0.3).rotate(1.0F, 0.0F, 0.0F, glm::pi<float>() * 0.5F);
        christmasBallEntity.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(christmasBallMaterial);

        size_t numSpheresX = 10;
        size_t numSpheresZ = 10;

        for (size_t i = 0; i < numSpheresX; ++i) {
            for (size_t j = 0; j < numSpheresZ; ++j) {

                MaterialConfiguration sphereMaterial1Config{};
                sphereMaterial1Config.device = Engine::graphics()->getDevice();
                sphereMaterial1Config.setAlbedo(glm::vec3(0.5F));
                sphereMaterial1Config.setRoughness(1.0F - (((float)i + 0.5F) / (float)numSpheresX));
                sphereMaterial1Config.setMetallic(1.0F - (((float)j + 0.5F) / (float)numSpheresX));
                std::shared_ptr<Material> sphereMaterial1 = std::shared_ptr<Material> (Material::create(sphereMaterial1Config, "Demo-SphereMaterial1-" + std::to_string(i) + "-" + std::to_string(j)));

                Entity sphereEntity1 = EntityHierarchy::create(Engine::scene(), "sphereEntity[" + std::to_string(i) + ", " + std::to_string(j) + "]");
                sphereEntity1.addComponent<Transform>().translate(-4.0F + i * 0.26F, 0.333F, 2.0F + j * 0.26F).scale(0.5F);
                sphereEntity1.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(sphereMaterial1);
            }
        }

        MaterialConfiguration glowMaterialConfig{};
        glowMaterialConfig.device = Engine::graphics()->getDevice();
        glowMaterialConfig.setAlbedo(glm::vec3(1.0F, 1.0F, 1.0F));
        glowMaterialConfig.setRoughness(1.0F);
        glowMaterialConfig.setMetallic(0.0F);

        Entity lightEntity1 = EntityHierarchy::create(Engine::scene(), "lightEntity1");
        lightEntity1.addComponent<Transform>().translate(3.0, 0.8, -1.0).scale(0.125F);
        lightEntity1.addComponent<LightComponent>().setType(LightType_Point).setIntensity(32.0, 8.0, 0.0);
        glowMaterialConfig.setEmission(glm::vec3(32.0, 8.0, 0.0));
        lightEntity1.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig, "Demo-LightEntity1-GlowMaterial")));

        Entity lightEntity2 = EntityHierarchy::create(Engine::scene(), "lightEntity2");
        lightEntity2.addComponent<Transform>().translate(0.4, 1.3, 2.0).scale(0.125F);
        lightEntity2.addComponent<LightComponent>().setType(LightType_Point).setIntensity(32.0F, 32.0F, 32.0F);
        glowMaterialConfig.setEmission(glm::vec3(32.0F, 32.0F, 32.0F));
        lightEntity2.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig, "Demo-LightEntity2-GlowMaterial")));

        Entity lightEntity3 = EntityHierarchy::create(Engine::scene(), "lightEntity3");
        lightEntity3.addComponent<Transform>().translate(-2.0, 1.1, -1.2).scale(0.125F);
        lightEntity3.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8F, 6.4F, 32.0F);
        glowMaterialConfig.setEmission(glm::vec3(0.8, 6.4, 32.0));
        lightEntity3.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig, "Demo-LightEntity3-GlowMaterial")));

        Entity lightEntity4 = EntityHierarchy::create(Engine::scene(), "lightEntity4");
        lightEntity4.addComponent<Transform>().translate(-2.1, 1.1, 2.3).scale(0.125F);
        lightEntity4.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8F, 32.0F, 6.4F);
        glowMaterialConfig.setEmission(glm::vec3(0.8F, 32.0F, 6.4F));
        lightEntity4.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig, "Demo-LightEntity4-GlowMaterial")));

        Entity lightEntity5 = EntityHierarchy::create(Engine::scene(), "lightEntity5");
        lightEntity5.addComponent<Transform>().translate(3.1, 1.1, 1.1).scale(0.125F);
        lightEntity5.addComponent<LightComponent>().setType(LightType_Point).setIntensity(0.8F, 32.0F, 41.0F);
        glowMaterialConfig.setEmission(glm::vec3(0.8F, 32.0F, 41.0F));
        lightEntity5.addComponent<RenderComponent>().setMesh(sphereMesh).setMaterial(std::shared_ptr<Material>(Material::create(glowMaterialConfig, "Demo-LightEntity5-GlowMaterial")));

        Entity lightEntity6 = EntityHierarchy::create(Engine::scene(), "lightEntity6");
        lightEntity6.addComponent<Transform>().setRotation(glm::vec3(-1.0F, -1.3F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
        lightEntity6.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(70.0F, 70.0F, 70.0F).setShadowCaster(true).setShadowCascadeDistances({3.0F, 6.0F, 12.0F, 24.0F});

//        Entity lightEntity7 = EntityHierarchy::create(Engine::scene(), "lightEntity7");
//        lightEntity7.addComponent<Transform>().setRotation(glm::vec3(-1.4F, -1.0F, 0.2F), glm::vec3(0.0F, 1.0F, 0.0F), false);
//        lightEntity7.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(70.0, 60.0, 10.0).setShadowCaster(true).setShadowCascadeDistances({3.0F, 6.0F, 12.0F, 24.0F});

        Engine::scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(0.0F, 1.0F, 1.0F);
    }

    void cleanup() override {

        textures.clear();
        for (int i = 0; i < images.size(); ++i)
            delete images[i];
    }

    void handleUserInput(const double& dt) {
        if (input()->keyPressed(SDL_SCANCODE_ESCAPE)) {
            input()->toggleMouseGrabbed();
        }

        if (input()->isMouseGrabbed()) {
            Transform& cameraTransform = Engine::scene()->getMainCameraEntity().getComponent<Transform>();
            glm::ivec2 dMouse = input()->getRelativeMouseState();
            if (isViewportInverted())
                dMouse.y *= -1;

            if (cameraMouseInputLocked) {
                cameraPitch = cameraTransform.getPitch();
                cameraYaw = cameraTransform.getYaw();
            } else {
                cameraPitch += dMouse.y * 0.001F;
                cameraYaw -= dMouse.x * 0.001F;
                if (cameraYaw > +M_PI) cameraYaw -= M_PI * 2.0;
                if (cameraYaw < -M_PI) cameraYaw += M_PI * 2.0;
                if (cameraPitch > +M_PI * 0.5) cameraPitch = +M_PI * 0.5;
                if (cameraPitch < -M_PI * 0.5) cameraPitch = -M_PI * 0.5;

                cameraTransform.setRotation((float) cameraPitch, (float) cameraYaw);
            }

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

    Frustum frustum;
    bool pauseFrustum = false;
    double time = 0.0;
    float framerateLimit = 0.0F;
    bool histogramNormalized = true;
    float test = 1.0F;

    void render(const double& dt) override {
        PROFILE_SCOPE("custom render")
        handleUserInput(dt);
        time += dt;

//        ImGui::ShowDemoWindow();

        bool taaEnabled = Engine::reprojectionRenderer()->isTaaEnabled();
        float taaHistoryFadeFactor = Engine::reprojectionRenderer()->getTaaHistoryFactor();
        uint32_t colourClippingMode = Engine::reprojectionRenderer()->getTaaColourClippingMode();
        bool useCatmullRomFilter = Engine::reprojectionRenderer()->getTaaUseCatmullRomFilter();
        bool useMitchellFilter = Engine::reprojectionRenderer()->getTaaUseMitchellFilter();
        glm::vec2 mitchellCoeffs = Engine::reprojectionRenderer()->getTaaMitchellFilterCoefficients();
        const char* taaColourClippingModeNames[3] = { "Clamp", "Fast Clipping", "Accurate Clipping" };

        bool bloomEnabled = Engine::postProcessingRenderer()->isBloomEnabled();
        float bloomBlurFilterRadius = Engine::postProcessingRenderer()->getBloomBlurFilterRadius();
        float bloomIntensity = Engine::postProcessingRenderer()->getBloomIntensity();
        float bloomThreshold = Engine::postProcessingRenderer()->getBloomThreshold();
        float bloomSoftThreshold = Engine::postProcessingRenderer()->getBloomSoftThreshold();
        float bloomBaxBrightness = Engine::postProcessingRenderer()->getBloomMaxBrightness();
        int bloomBlurIterations = (int)Engine::postProcessingRenderer()->getBloomBlurIterations() - 1;
        int bloomBlurMaxIterations = (int)Engine::postProcessingRenderer()->getMaxBloomBlurIterations() - 1;

        uint32_t histogramDownsampleFactor = Engine::postProcessingRenderer()->exposureHistogram()->getDownsampleFactor();
        float histogramOffset = Engine::postProcessingRenderer()->exposureHistogram()->getOffset();
        float histogramScale = Engine::postProcessingRenderer()->exposureHistogram()->getScale();
        float histogramLowPercent = Engine::postProcessingRenderer()->exposureHistogram()->getLowPercent() * 100.0F;
        float histogramHighPercent = Engine::postProcessingRenderer()->exposureHistogram()->getHighPercent() * 100.0F;
        float exposureSpeedUp = Engine::postProcessingRenderer()->exposureHistogram()->getExposureSpeedUp();
        float exposureSpeedDown = Engine::postProcessingRenderer()->exposureHistogram()->getExposureSpeedDown();
        float exposureCompensation = Engine::postProcessingRenderer()->exposureHistogram()->getExposureCompensation();

        ImGui::Begin("Test");
        if (ImGui::CollapsingHeader("Temporal AA")) {
            ImGui::Checkbox("Enabled", &taaEnabled);
            ImGui::BeginDisabled(!taaEnabled);
            if (ImGui::BeginCombo("Colour clipping mode", taaColourClippingModeNames[colourClippingMode])) {
                for (uint32_t i = 0; i < 3; ++i)
                    if (ImGui::Selectable(taaColourClippingModeNames[i], colourClippingMode == i))
                        colourClippingMode = i;
                ImGui::EndCombo();
            }
            ImGui::Checkbox("Use CatmullRom filter", &useCatmullRomFilter);
            ImGui::SameLine();
            ImGui::Checkbox("Use Mitchell Filter", &useMitchellFilter);
            ImGui::BeginDisabled(!useMitchellFilter);
            ImGui::SliderFloat("Mitchell B", &mitchellCoeffs[0], -2.0F, +2.0F);
            ImGui::SliderFloat("Mitchell C", &mitchellCoeffs[1], 0.0F, +4.0F);
            ImGui::EndDisabled();
            ImGui::SliderFloat("History Fade Factor", &taaHistoryFadeFactor, 0.0F, 1.0F);
            ImGui::EndDisabled();
        }
        if (ImGui::CollapsingHeader("Bloom")) {
            ImGui::Checkbox("Enabled", &bloomEnabled);
            ImGui::BeginDisabled(!bloomEnabled);
            ImGui::SliderFloat("Filter radius", &bloomBlurFilterRadius, 0.0F, 30.0F, "%.5f");
            ImGui::SliderFloat("Intensity", &bloomIntensity, 0.0F, 1.0F, "%.5f");
            ImGui::SliderFloat("Threshold", &bloomThreshold, 0.0F, 30.0F, "%.5f");
            ImGui::SliderFloat("Soft Threshold", &bloomSoftThreshold, 0.0F, 1.0F, "%.5f");
            ImGui::SliderFloat("Max Brightness", &bloomBaxBrightness, 0.0F, 100.0F, "%.5f");
            ImGui::SliderInt("Iterations", &bloomBlurIterations, 1, bloomBlurMaxIterations);
            ImGui::EndDisabled();
        }
        if (ImGui::CollapsingHeader("Exposure")) {
            ImGui::Checkbox("Histogram Normalized", &histogramNormalized);
            ImGui::DragFloat("Histogram Offset", &histogramOffset, 0.005F, -2.0F, 2.0F);
            ImGui::DragFloat("Histogram Scale", &histogramScale, 0.005F, 0.005F, 2.0F);
            ImGui::DragFloat("Histogram Low Percent", &histogramLowPercent, 0.1F, 0.0F, histogramHighPercent);
            ImGui::DragFloat("Histogram High Percent", &histogramHighPercent, 0.1F, histogramLowPercent, 100.0F);
            ImGui::DragFloat("Exposure Speed Up", &exposureSpeedUp, 0.005F, 0.0F, 20.0F, "%.5f");
            ImGui::DragFloat("Exposure Speed Down", &exposureSpeedDown, 0.005F, 0.0F, 20.0F, "%.5f");
            ImGui::DragFloat("Exposure Compensation", &exposureCompensation, 0.005F, -8.0F, 8.0F);
        }
        if (ImGui::CollapsingHeader("Misc")) {
            ImGui::SliderFloat("Framerate Limit", &framerateLimit, 0.0F, 200.0F);
            ImGui::SliderFloat("Movement speed", &playerMovementSpeed, 0.05F, 10.0F);
            ImGui::Checkbox("Look at cube", &cameraMouseInputLocked);
        }
        ImGui::End();

        taaHistoryFadeFactor = glm::floor(taaHistoryFadeFactor / 0.01F) * 0.01F;
        framerateLimit = glm::floor(framerateLimit / 5.0F) * 5.0F;

        Application::instance()->setFramerateLimit(framerateLimit);

        Engine::reprojectionRenderer()->setTaaEnabled(taaEnabled);
        Engine::reprojectionRenderer()->setTaaHistoryFactor(taaHistoryFadeFactor);
        Engine::reprojectionRenderer()->setTaaUseCatmullRomFilter(useCatmullRomFilter);
        Engine::reprojectionRenderer()->setTaaColourClippingMode((ReprojectionRenderer::ColourClippingMode)colourClippingMode);
        Engine::reprojectionRenderer()->setTaaUseMitchellFilter(useMitchellFilter);
        Engine::reprojectionRenderer()->setTaaMitchellFilterCoefficients(mitchellCoeffs[0], mitchellCoeffs[1]);

        Engine::postProcessingRenderer()->setBloomEnabled(bloomEnabled);
        Engine::postProcessingRenderer()->setBloomBlurFilterRadius(bloomBlurFilterRadius);
        Engine::postProcessingRenderer()->setBloomIntensity(bloomIntensity);
        Engine::postProcessingRenderer()->setBloomThreshold(bloomThreshold);
        Engine::postProcessingRenderer()->setBloomSoftThreshold(bloomSoftThreshold);
        Engine::postProcessingRenderer()->setBloomMaxBrightness(bloomBaxBrightness);
        Engine::postProcessingRenderer()->setBloomBlurIterations(bloomBlurIterations + 1);

        Engine::postProcessingRenderer()->exposureHistogram()->setDownsampleFactor(histogramDownsampleFactor);
        Engine::postProcessingRenderer()->exposureHistogram()->setOffset(histogramOffset);
        Engine::postProcessingRenderer()->exposureHistogram()->setScale(histogramScale);
        Engine::postProcessingRenderer()->exposureHistogram()->setLowPercent(histogramLowPercent * 0.01F);
        Engine::postProcessingRenderer()->exposureHistogram()->setHighPercent(histogramHighPercent * 0.01F);
        Engine::postProcessingRenderer()->exposureHistogram()->setExposureSpeedUp(exposureSpeedUp);
        Engine::postProcessingRenderer()->exposureHistogram()->setExposureSpeedDown(exposureSpeedDown);
        Engine::postProcessingRenderer()->exposureHistogram()->setExposureCompensation(exposureCompensation);

        if (histogramNormalized) {
            test = glm::max(0.0F, test - (float)dt);
        } else {
            test = glm::min(1.0F, test + (float)dt);
        }
        Engine::postProcessingRenderer()->setTest(test);

        Entity mainCamera = Engine::scene()->getMainCameraEntity();
        Transform& cameraTransform = mainCamera.getComponent<Transform>();
        Camera& cameraProjection = mainCamera.getComponent<Camera>();

        if (cameraMouseInputLocked) {
            Entity cubeEntity = Engine::scene()->findNamedEntity("cubeEntity");
            cameraTransform.setRotation(cubeEntity.getComponent<Transform>().getTranslation() - cameraTransform.getTranslation(),glm::vec3(0, 1, 0), false);
        }

        Entity christmasBallEntity = Engine::scene()->findNamedEntity("christmasBallEntity");
        if (christmasBallEntity) {
            Transform& transform = christmasBallEntity.getComponent<Transform>();
            transform.rotate(0.0F, 0.0F, 1.0F, glm::two_pi<float>() * (float)dt * 0.25F);
            transform.translate(0.0F, glm::sin(time * 2.0F) * dt * 0.333F, 0.0F);
        }

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