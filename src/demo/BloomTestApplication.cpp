
#include "BloomTestApplication.h"
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
#include "core/engine/scene/bound/Frustum.h"
#include "core/engine/renderer/LightComponent.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/renderer/renderPasses/ReprojectionRenderer.h"
#include "core/engine/renderer/renderPasses/PostProcessRenderer.h"
#include "core/engine/renderer/renderPasses/ExposureHistogram.h"
#include "core/engine/renderer/EnvironmentMap.h"
#include "extern/imgui/imgui.h"
#include "core/engine/physics/RigidBody.h"


BloomTestApplication::BloomTestApplication():
    frustum(new Frustum()) {
}

BloomTestApplication::~BloomTestApplication() {
    delete frustum;
}

void BloomTestApplication::init() {
    //eventDispatcher()->connect<ScreenResizeEvent>(&App::onScreenResize, this);

    setTickrate(60.0);

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
    floorMaterialConfig.setAlbedoMap(loadTexture("textures/blacktiles04/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
    floorMaterialConfig.setRoughnessMap(loadTexture("textures/blacktiles04/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        floorMaterialConfig.setMetallicMap(loadTexture("textures/blacktiles04/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
    floorMaterialConfig.setNormalMap(loadTexture("textures/blacktiles04/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
    std::shared_ptr<Material> floorMaterial = std::shared_ptr<Material>(Material::create(floorMaterialConfig, "Demo-FloorMaterial"));

    MaterialConfiguration cubeMaterialConfig{};
    cubeMaterialConfig.device = Engine::graphics()->getDevice();
    cubeMaterialConfig.setAlbedoMap(loadTexture("textures/mossybark02/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
    cubeMaterialConfig.setRoughnessMap(loadTexture("textures/mossybark02/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
//        cubeMaterialConfig.setMetallicMap(loadTexture("textures/mossybark02/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
    cubeMaterialConfig.setNormalMap(loadTexture("textures/mossybark02/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
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
    MeshUtils::loadMeshData("meshes/bunny.obj", testMeshData);
    glm::vec3 centerBottom = testMeshData.calculateBoundingBox() * glm::vec4(0, -1, 0, 1);
    testMeshData.translate(-1.0F * centerBottom);
    testMeshData.applyTransform();
    testMeshData.computeTangents();
//
    LOG_INFO("Loaded bunny.obj :- %zu polygons", testMeshData.getPolygonCount());
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
    christmasBallMaterialConfig.setAlbedoMap(loadTexture("textures/christmas_tree_ball/albedo.png", vk::Format::eR8G8B8A8Unorm, sampler));
    christmasBallMaterialConfig.setRoughnessMap(loadTexture("textures/christmas_tree_ball/roughness.png", vk::Format::eR8G8B8A8Unorm, sampler));
    christmasBallMaterialConfig.setMetallicMap(loadTexture("textures/christmas_tree_ball/metallic.png", vk::Format::eR8G8B8A8Unorm, sampler));
    christmasBallMaterialConfig.setNormalMap(loadTexture("textures/christmas_tree_ball/normal.png", vk::Format::eR8G8B8A8Unorm, sampler));
    christmasBallMaterialConfig.setDisplacementMap(loadTexture("textures/christmas_tree_ball/displacement.png", vk::Format::eR8G8B8A8Unorm, sampler));
    std::shared_ptr<Material> christmasBallMaterial = std::shared_ptr<Material>(Material::create(christmasBallMaterialConfig, "Demo-ChristmasBallMaterial"));

    Entity christmasBallEntity = EntityHierarchy::create(Engine::scene(), "christmasBallEntity");
    christmasBallEntity.addComponent<RigidBody>().setInterpolationType(RigidBody::InterpolationType_Extrapolate)
            .transform().translate(-2.0, 0.6, 0.3).rotate(1.0F, 0.0F, 0.0F, glm::pi<float>() * 0.5F);
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
    lightEntity6.addComponent<Transform>().setRotation(glm::vec3(-1.333F, -0.90F, -1.0F), glm::vec3(0.0F, 1.0F, 0.0F), false);
    glm::vec3 sunIntensity = glm::vec3(100.0F);
    lightEntity6.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(sunIntensity).setAngularSize(glm::radians(0.52F)).setShadowCaster(true).setShadowCascadeDistances({3.0F, 6.0F, 12.0F, 24.0F});

    Entity lightEntity7 = EntityHierarchy::create(Engine::scene(), "lightEntity7");
    lightEntity7.addComponent<Transform>().setRotation(glm::vec3(-1.4F, -1.0F, 0.2F), glm::vec3(0.0F, 1.0F, 0.0F), false);
    lightEntity7.addComponent<LightComponent>().setType(LightType_Directional).setIntensity(70.0, 30.0, 10.0).setAngularSize(glm::radians(0.21F)).setShadowCaster(true).setShadowCascadeDistances({3.0F, 6.0F, 12.0F, 24.0F});

    Engine::scene()->getMainCameraEntity().getComponent<Transform>().setTranslation(0.0F, 1.0F, 1.0F);

    ImageCubeConfiguration imageCubeConfig{};
    imageCubeConfig.device = Engine::graphics()->getDevice();
    imageCubeConfig.format = vk::Format::eR32G32B32A32Sfloat;
    imageCubeConfig.usage = vk::ImageUsageFlagBits::eSampled;
    imageCubeConfig.generateMipmap = true;
    imageCubeConfig.mipLevels = UINT32_MAX;
    imageCubeConfig.imageSource.setEquirectangularSource("environment_maps/wide_street_02_8k_nosun.hdr");
    std::shared_ptr<ImageCube> skyboxImageCube = std::shared_ptr<ImageCube>(ImageCube::create(imageCubeConfig, "SkyboxCubeImage"));

    std::shared_ptr<EnvironmentMap> skyboxEnvironmentMap = std::make_shared<EnvironmentMap>(skyboxImageCube);
    skyboxEnvironmentMap->update();

    Engine::instance()->getDeferredRenderer()->setEnvironmentMap(skyboxEnvironmentMap);
}

void BloomTestApplication::cleanup() {

    textures.clear();
    for (auto& image : images)
        delete image;
}

void BloomTestApplication::render(double dt) {
    PROFILE_SCOPE("custom render")
    handleUserInput(dt);

//        ImGui::ShowDemoWindow();

    bool taaEnabled = Engine::instance()->getReprojectionRenderer()->isTaaEnabled();
    float taaHistoryFadeFactor = Engine::instance()->getReprojectionRenderer()->getTaaHistoryFactor();
    uint32_t colourClippingMode = Engine::instance()->getReprojectionRenderer()->getTaaColourClippingMode();
    bool useCatmullRomFilter = Engine::instance()->getReprojectionRenderer()->getTaaUseCatmullRomFilter();
    bool useMitchellFilter = Engine::instance()->getReprojectionRenderer()->getTaaUseMitchellFilter();
    glm::vec2 mitchellCoeffs = Engine::instance()->getReprojectionRenderer()->getTaaMitchellFilterCoefficients();
    const char* taaColourClippingModeNames[3] = { "Clamp", "Fast Clipping", "Accurate Clipping" };

    bool bloomEnabled = Engine::instance()->getPostProcessingRenderer()->isBloomEnabled();
    float bloomBlurFilterRadius = Engine::instance()->getPostProcessingRenderer()->getBloomBlurFilterRadius();
    float bloomIntensity = Engine::instance()->getPostProcessingRenderer()->getBloomIntensity();
    float bloomThreshold = Engine::instance()->getPostProcessingRenderer()->getBloomThreshold();
    float bloomSoftThreshold = Engine::instance()->getPostProcessingRenderer()->getBloomSoftThreshold();
    float bloomBaxBrightness = Engine::instance()->getPostProcessingRenderer()->getBloomMaxBrightness();
    int bloomBlurIterations = (int)Engine::instance()->getPostProcessingRenderer()->getBloomBlurIterations() - 1;
    int bloomBlurMaxIterations = (int)Engine::instance()->getPostProcessingRenderer()->getMaxBloomBlurIterations() - 1;

    uint32_t histogramDownsampleFactor = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getDownsampleFactor();
    float histogramMinLogLum = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getMinLogLuminance();
    float histogramLogLumRange = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getLogLuminanceRange();
    float histogramLowPercent = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getLowPercent() * 100.0F;
    float histogramHighPercent = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getHighPercent() * 100.0F;
    float exposureSpeedUp = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getExposureSpeedUp();
    float exposureSpeedDown = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getExposureSpeedDown();
    float exposureCompensation = Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->getExposureCompensation();

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
        ImGui::DragFloat("Histogram log2(lum) Min", &histogramMinLogLum, 0.05F, -30.0F, 10.0F);
        ImGui::DragFloat("Histogram log2(lum) Range", &histogramLogLumRange, 0.05F, 1.0F, 40.0F);
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

    Engine::instance()->getReprojectionRenderer()->setTaaEnabled(taaEnabled);
    Engine::instance()->getReprojectionRenderer()->setTaaHistoryFactor(taaHistoryFadeFactor);
    Engine::instance()->getReprojectionRenderer()->setTaaUseCatmullRomFilter(useCatmullRomFilter);
    Engine::instance()->getReprojectionRenderer()->setTaaColourClippingMode((ReprojectionRenderer::ColourClippingMode)colourClippingMode);
    Engine::instance()->getReprojectionRenderer()->setTaaUseMitchellFilter(useMitchellFilter);
    Engine::instance()->getReprojectionRenderer()->setTaaMitchellFilterCoefficients(mitchellCoeffs[0], mitchellCoeffs[1]);

    Engine::instance()->getPostProcessingRenderer()->setBloomEnabled(bloomEnabled);
    Engine::instance()->getPostProcessingRenderer()->setBloomBlurFilterRadius(bloomBlurFilterRadius);
    Engine::instance()->getPostProcessingRenderer()->setBloomIntensity(bloomIntensity);
    Engine::instance()->getPostProcessingRenderer()->setBloomThreshold(bloomThreshold);
    Engine::instance()->getPostProcessingRenderer()->setBloomSoftThreshold(bloomSoftThreshold);
    Engine::instance()->getPostProcessingRenderer()->setBloomMaxBrightness(bloomBaxBrightness);
    Engine::instance()->getPostProcessingRenderer()->setBloomBlurIterations(bloomBlurIterations + 1);

    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setDownsampleFactor(histogramDownsampleFactor);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setMinLogLuminance(histogramMinLogLum);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setLogLuminanceRange(histogramLogLumRange);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setLowPercent(histogramLowPercent * 0.01F);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setHighPercent(histogramHighPercent * 0.01F);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setExposureSpeedUp(exposureSpeedUp);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setExposureSpeedDown(exposureSpeedDown);
    Engine::instance()->getPostProcessingRenderer()->exposureHistogram()->setExposureCompensation(exposureCompensation);

    if (histogramNormalized) {
        test = glm::max(0.0F, test - (float)dt);
    } else {
        test = glm::min(1.0F, test + (float)dt);
    }
    Engine::instance()->getPostProcessingRenderer()->setTest(test);

    Entity mainCamera = Engine::scene()->getMainCameraEntity();
    Transform& cameraTransform = mainCamera.getComponent<Transform>();
    Camera& cameraProjection = mainCamera.getComponent<Camera>();

    if (cameraMouseInputLocked) {
        Entity cubeEntity = Engine::scene()->findNamedEntity("cubeEntity");
        cameraTransform.setRotation(cubeEntity.getComponent<Transform>().getTranslation() - cameraTransform.getTranslation(),glm::vec3(0, 1, 0), false);
    }

    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_Projection);
    Engine::instance()->getImmediateRenderer()->pushMatrix("BloomTestApplication::render/Projection");
    Engine::instance()->getImmediateRenderer()->loadMatrix(cameraProjection.getProjectionMatrix());
    Engine::instance()->getImmediateRenderer()->matrixMode(MatrixMode_ModelView);
    Engine::instance()->getImmediateRenderer()->pushMatrix("BloomTestApplication::render/ModelView");
    Engine::instance()->getImmediateRenderer()->loadMatrix(glm::inverse(glm::mat4(cameraTransform.getMatrix())));

    Engine::instance()->getImmediateRenderer()->setCullMode(vk::CullModeFlagBits::eNone);
    Engine::instance()->getImmediateRenderer()->setColourBlendMode(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);

    if (input()->keyPressed(SDL_SCANCODE_F)) {
        if (!pauseFrustum) {
            pauseFrustum = true;
            frustum->set(cameraTransform, cameraProjection);
        } else {
            pauseFrustum = false;
        }
    }

    if (pauseFrustum) {
        Engine::instance()->getImmediateRenderer()->setBlendEnabled(true);
        Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(true);
        Engine::instance()->getImmediateRenderer()->colour(1.0F, 0.0F, 0.0F, 0.25F);
        frustum->drawFill();

        Engine::instance()->getImmediateRenderer()->setLineWidth(1.0F);
        Engine::instance()->getImmediateRenderer()->setBlendEnabled(false);
        Engine::instance()->getImmediateRenderer()->setDepthTestEnabled(false);
        Engine::instance()->getImmediateRenderer()->colour(1.0F, 1.0F, 1.0F, 1.0F);
        frustum->drawLines();
    }

    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_ModelView, "BloomTestApplication::render/ModelView");
    Engine::instance()->getImmediateRenderer()->popMatrix(MatrixMode_Projection, "BloomTestApplication::render/Projection");
}

void BloomTestApplication::tick(double dt) {
    time += dt;

    Entity christmasBallEntity = Engine::scene()->findNamedEntity("christmasBallEntity");
    if (christmasBallEntity) {
        RigidBody& rigidBody = christmasBallEntity.getComponent<RigidBody>();
        rigidBody.transform().rotate(0.0F, 0.0F, 1.0F, glm::two_pi<float>() * (float)dt * 0.25F);
        rigidBody.transform().translate(0.0F, glm::sin(time * 2.0F) * dt * 0.333F, 0.0F);
    }
}

void BloomTestApplication::handleUserInput(double dt) {
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

        if (cameraMouseInputLocked) {
            cameraPitch = cameraTransform.getPitch();
            cameraYaw = cameraTransform.getYaw();
        } else {
            const float mouseSensitivity = 0.001F / currentZoomFactor;
            cameraPitch += dMouse.y * mouseSensitivity;
            cameraYaw -= dMouse.x * mouseSensitivity;
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

//    LOG_DEBUG("targetZoomFactor = %.2f, currentZoomFactor = %.2f, fov = %.2f deg", targetZoomFactor, currentZoomFactor, glm::degrees(camera.getFov()));

}

std::shared_ptr<Texture> BloomTestApplication::loadTexture(const std::string& filePath, vk::Format format, const std::weak_ptr<Sampler>& sampler) {
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