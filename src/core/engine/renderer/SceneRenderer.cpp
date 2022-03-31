#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/scene/event/EventDispacher.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/Texture.h"
#include "core/thread/ThreadUtils.h"
#include "core/util/Profiler.h"
#include <thread>

//#include "../thread/ThreadUtils.h"

SceneRenderer::SceneRenderer() {
}

SceneRenderer::~SceneRenderer() {
    m_missingTexture.reset();
    m_missingTextureImage.reset();
    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->cameraInfoBuffer;
        delete m_resources[i]->worldTransformBuffer;
        delete m_resources[i]->globalDescriptorSet;
        delete m_resources[i]->objectDescriptorSet;
        delete m_resources[i]->materialDescriptorSet;
    }
}

void SceneRenderer::init() {
    initMissingTexture();

    std::shared_ptr<DescriptorPool> descriptorPool = Application::instance()->graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    constexpr size_t maxTextures = 0xFFFF;

    std::vector<Texture2D*> initialTextures;
    initialTextures.resize(maxTextures, m_missingTexture.get());

    std::vector<vk::ImageLayout> initialImageLayouts;
    initialImageLayouts.resize(maxTextures, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_globalDescriptorSetLayout = builder.addUniformBlock(0, vk::ShaderStageFlagBits::eVertex, sizeof(CameraInfoUBO)).build();
    m_objectDescriptorSetLayout = builder.addStorageBlock(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, sizeof(ObjectDataUBO)).build();
    m_materialDescriptorSetLayout = builder.addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, maxTextures).build();

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->globalDescriptorSet = DescriptorSet::create(m_globalDescriptorSetLayout, descriptorPool);
        m_resources[i]->objectDescriptorSet = DescriptorSet::create(m_objectDescriptorSetLayout, descriptorPool);
        m_resources[i]->materialDescriptorSet = DescriptorSet::create(m_materialDescriptorSetLayout, descriptorPool);

        BufferConfiguration cameraInfoBufferConfig;
        cameraInfoBufferConfig.device = Application::instance()->graphics()->getDevice();
        cameraInfoBufferConfig.size = sizeof(CameraInfoUBO);
        cameraInfoBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        cameraInfoBufferConfig.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        m_resources[i]->cameraInfoBuffer = Buffer::create(cameraInfoBufferConfig);

        DescriptorSetWriter(m_resources[i]->globalDescriptorSet)
                .writeBuffer(0, m_resources[i]->cameraInfoBuffer, 0, m_resources[i]->cameraInfoBuffer->getSize()).write();

        DescriptorSetWriter(m_resources[i]->materialDescriptorSet)
                .writeImage(0, initialTextures.data(), initialImageLayouts.data(), maxTextures, 0).write();
    }

    m_scene->enableEvents<RenderComponent>();
    m_scene->getEventDispacher()->connect<ComponentAddedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentAdded, this);
    m_scene->getEventDispacher()->connect<ComponentRemovedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentRemoved, this);
}

void SceneRenderer::render(double dt) {
    PROFILE_SCOPE("SceneRenderer::render")
    const Entity& cameraEntity = m_scene->getMainCamera();
    m_renderCamera.setProjection(cameraEntity.getComponent<Camera>());
    m_renderCamera.setTransform(cameraEntity.getComponent<Transform>());
    m_renderCamera.update();

    render(dt, &m_renderCamera);
}

void SceneRenderer::render(double dt, RenderCamera* renderCamera) {
    PROFILE_SCOPE("SceneRenderer::render")
    const uint32_t currentFrameIndex = Application::instance()->graphics()->getCurrentFrameIndex();

    CameraInfoUBO cameraInfo;
    cameraInfo.viewProjectionMatrix = renderCamera->getViewProjectionMatrix();
    m_resources->cameraInfoBuffer->upload(0, sizeof(CameraInfoUBO), &cameraInfo);

    if (m_needsSortRenderableEntities) {
        m_needsSortRenderableEntities = false;
        sortRenderableEntities();
    }

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);
    m_numRenderEntities = renderEntities.size();
    mappedWorldTransformsBuffer(m_numRenderEntities);

    updateMaterialsBuffer();
    updateEntityWorldTransforms();
    streamObjectData();


    const vk::CommandBuffer& commandBuffer = Application::instance()->graphics()->getCurrentCommandBuffer();
    recordRenderCommands(dt, commandBuffer);
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}

void SceneRenderer::initPipelineDescriptorSetLayouts(GraphicsPipelineConfiguration& graphicsPipelineConfiguration) const {
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_globalDescriptorSetLayout->getDescriptorSetLayout());
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_objectDescriptorSetLayout->getDescriptorSetLayout());
    graphicsPipelineConfiguration.descriptorSetLayous.emplace_back(m_materialDescriptorSetLayout->getDescriptorSetLayout());
}

void SceneRenderer::recordRenderCommands(double dt, vk::CommandBuffer commandBuffer) {
    PROFILE_SCOPE("SceneRenderer::recordRenderCommands")

    PROFILE_REGION("Bind resources")
    GraphicsManager* graphics = Application::instance()->graphics();
    const uint32_t currentFrameIndex = graphics->getCurrentFrameIndex();
    const vk::Device& device = **graphics->getDevice();


    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    std::shared_ptr<GraphicsPipeline> pipeline = Application::instance()->graphics()->pipeline();


    std::vector<vk::DescriptorSet> descriptorSets = {
            m_resources->globalDescriptorSet->getDescriptorSet(),
            m_resources->objectDescriptorSet->getDescriptorSet(),
            m_resources->materialDescriptorSet->getDescriptorSet(),
    };

    std::vector<uint32_t> dynamicOffsets = {};

    graphics->pipeline()->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics->pipeline()->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);

    uint32_t firstInstance = 0;
    uint32_t instanceCount = 0;
    Mesh* currMesh = NULL;

    PROFILE_REGION("Draw meshes")
    for (auto id : renderEntities) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);

        if (currMesh == NULL) {
            currMesh = renderComponent.mesh.get();
            instanceCount = 0;
            firstInstance = 0;

        } else if (currMesh != renderComponent.mesh.get()) {
            currMesh->draw(commandBuffer, instanceCount, firstInstance);
            currMesh = renderComponent.mesh.get();
            firstInstance += instanceCount;
            instanceCount = 0;
        }

        ++instanceCount;
    }

    if (currMesh != NULL) {
        currMesh->draw(commandBuffer, instanceCount, firstInstance);
    }

}

void SceneRenderer::initMissingTexture() {
    union {
        glm::u8vec4 pixels[2][2];
        uint8_t pixelBytes[sizeof(glm::u8vec4) * 4];
    };

    pixels[0][0] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);
    pixels[0][1] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][0] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][1] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);

    ImageData imageData(pixelBytes, 2, 2, ImagePixelLayout::RGBA, ImagePixelFormat::UInt8);

    Image2DConfiguration missingTextureImageConfig;
    missingTextureImageConfig.device = Application::instance()->graphics()->getDevice();
    missingTextureImageConfig.format = vk::Format::eR8G8B8A8Unorm;
    missingTextureImageConfig.imageData = &imageData;
    m_missingTextureImage = std::shared_ptr<Image2D>(Image2D::create(missingTextureImageConfig));

    ImageView2DConfiguration missingTextureImageViewConfig;
    missingTextureImageViewConfig.device = missingTextureImageConfig.device;
    missingTextureImageViewConfig.format = missingTextureImageConfig.format;
    missingTextureImageViewConfig.image = m_missingTextureImage->getImage();

    SamplerConfiguration missingTextureSamplerConfig;
    missingTextureSamplerConfig.device = missingTextureImageConfig.device;
    missingTextureSamplerConfig.minFilter = vk::Filter::eNearest;
    missingTextureSamplerConfig.magFilter = vk::Filter::eNearest;
    m_missingTexture = std::shared_ptr<Texture2D>(Texture2D::create(missingTextureImageViewConfig, missingTextureSamplerConfig));
}

void SceneRenderer::sortRenderableEntities() {
    PROFILE_SCOPE("SceneRenderer::sortRenderableEntities")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    // Group all meshes of the same type to be contiguous in the array
    renderEntities.sort([&renderEntities](const entt::entity& lhs, const entt::entity& rhs) {
        const RenderComponent& lhsRC = renderEntities.get<RenderComponent>(lhs);
        const RenderComponent& rhsRC = renderEntities.get<RenderComponent>(rhs);
        return lhsRC.mesh->getResourceId() < rhsRC.mesh->getResourceId();
    });

    Transform::ensureCapacity(renderEntities.size() - 1);

    size_t index = 0;
    for (auto id : renderEntities) {
        Transform& transform = renderEntities.get<Transform>(id);
        Transform::reindex(transform, index++);
    }
    //ThreadUtils::parallel_range(renderEntities.size(), [this, &renderEntities](size_t rangeStart, size_t rangeEnd) {
    //
    //    auto it = renderEntities.begin() + rangeStart;
    //    for (size_t index = rangeStart; index != rangeEnd; ++index, ++it) {
    //        Transform& transform = renderEntities.get<Transform>(*it);
    //        Transform::reindex(transform, index);
    //    }
    //});
}

void SceneRenderer::updateEntityWorldTransforms() {
    PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms")


    auto thread_exec = [this](
            size_t rangeStart,
            size_t rangeEnd) {

        PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms/thread_exec")

        const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

        const auto& resources = m_resources;

        //printf("Updating world entity transforms for entities [%llu to %llu]\n", rangeStart, rangeEnd);

        size_t maxDebugCounter = 256;
        size_t debugCounter = 0;

        auto it = renderEntities.begin() + rangeStart;
        for (size_t index = rangeStart; index != rangeEnd; ++index, ++it) {

            if (debugCounter == 0) {
                PROFILE_REGION("Debug 256 entities");
            }

            auto id = *it;
            Transform& transform = renderEntities.get<Transform>(id);

            if (Transform::hasChanged(index)) {
                PROFILE_SCOPE("Update transform")
                transform.update();

                for (int i = 0; i < CONCURRENT_FRAMES; ++i)
                    if (index < resources.get(i)->objectEntities.size())
                        resources.get(i)->objectEntities[index] = entt::null;
            }

            if (resources->objectEntities[index] != id) {
                PROFILE_SCOPE("Upload matrix")
                resources->objectEntities[index] = id;
                transform.fillMatrix(resources->objectBuffer[index].modelMatrix);
            }

            ++debugCounter;

            if (debugCounter == maxDebugCounter) {
                debugCounter = 0;
                PROFILE_REGION("Debug 256 entities end");
            }
        }
        if (debugCounter > 0) {
            PROFILE_REGION("Debug 256 entities end");
        }
    };

    //auto t0 = Performance::now();

    //thread_exec(0, m_numRenderEntities);
    ThreadUtils::parallel_range(m_numRenderEntities, thread_exec);

    //auto t1 = Performance::now();
    //double msec = Performance::milliseconds(t0, t1);
    //if (msec > 6)
    //    printf("updateEntityWorldTransforms Took %.4f msec\n", msec);
}

void SceneRenderer::streamObjectData() {
    PROFILE_SCOPE("SceneRenderer::streamObjectData")

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);
    ObjectDataUBO* objectBufferMap = mappedWorldTransformsBuffer(renderEntities.size());

    size_t numCopyBytes = sizeof(ObjectDataUBO) * renderEntities.size();
    memcpy(objectBufferMap, m_resources->objectBuffer.data(), numCopyBytes);
}

void SceneRenderer::updateMaterialsBuffer() {
    PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer")

    uint32_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

    //ObjectDataUBO* objectDataBuffer = mappedWorldTransformsBuffer(renderEntities.size());

    std::vector<Texture2D*>& objectTextures = m_resources->objectTextures;
    std::vector<Texture2D*>& materialBufferTextures = m_resources->materialBufferTextures;

    uint32_t textureIndex;
    size_t objectIndex = 0;
    uint32_t firstNewTextureIndex = (uint32_t)(-1);
    uint32_t lastNewTextureIndex = (uint32_t)(-1);
    Texture2D* texture;

    size_t numChangedTextures = 0;

    //auto t0 = Performance::now();

    //ThreadUtils::parallel_range(renderEntities.size(), [this](size_t rangeStart, size_t rangeEnd) {
    //    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);
    //    std::vector<Texture2D*>& objectTextures = m_resources->objectTextures;
    //
    //    auto it = renderEntities.begin() + rangeStart;
    //
    //    Texture2D* texture;
    //
    //    for (size_t index = rangeStart; index != rangeEnd; ++index, ++it) {
    //        auto id = *it;
    //        RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);
    //
    //        texture = renderComponent.texture.get();
    //        if (texture == NULL)
    //            texture = m_missingTexture.get();
    //
    //        if (objectTextures[index] != texture) {
    //            renderComponent.flags.textureChanged = true;
    //        } else {
    //            renderComponent.flags.textureChanged = false;
    //        }
    //    }
    //}, &renderEntities, &objectTextures);

    for (auto id : renderEntities) {
        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);

        texture = renderComponent.texture.get();
        if (texture == NULL)
            texture = m_missingTexture.get();

        if (objectTextures[objectIndex] != texture) {
            objectTextures[objectIndex] = texture;

            ++numChangedTextures;

            auto it = m_textureDescriptorIndices.find(texture);
            if (it == m_textureDescriptorIndices.end()) {
                textureIndex = materialBufferTextures.size();
                materialBufferTextures.emplace_back(texture);

                lastNewTextureIndex = textureIndex;
                if (firstNewTextureIndex == (uint32_t)(-1))
                    firstNewTextureIndex = textureIndex;

                m_textureDescriptorIndices.insert(std::make_pair(texture, textureIndex));
            } else {
                textureIndex = it->second;
            }

            m_resources->objectBuffer[objectIndex].textureIndex = textureIndex;
        }

        ++objectIndex;
    }

    //auto t1 = Performance::now();
    //printf("Took %.4f msec to update %llu changed textures in %llu entities\n", Performance::milliseconds(t0, t1), numChangedTextures, renderEntities.size());
    
    uint32_t arrayCount = glm::min(lastNewTextureIndex - firstNewTextureIndex, descriptorCount);
    if (arrayCount > 0) {

        std::vector<vk::ImageLayout>& materialBufferImageLayouts = m_resources->materialBufferImageLayouts;
        if (materialBufferImageLayouts.size() < arrayCount)
            materialBufferImageLayouts.resize(arrayCount, vk::ImageLayout::eShaderReadOnlyOptimal);

        printf("Writing textures [%d to %d]\n", firstNewTextureIndex, lastNewTextureIndex);
        DescriptorSetWriter(m_resources->materialDescriptorSet)
                .writeImage(0, &materialBufferTextures[firstNewTextureIndex], &materialBufferImageLayouts[0], arrayCount, firstNewTextureIndex)
                .write();
    }
}

void SceneRenderer::onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event) {
    //printf("SceneRenderer::onRenderComponentAdded");
    m_needsSortRenderableEntities = true;
    event.entity.addComponent<EntityRenderState>();
}

void SceneRenderer::onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event) {
    //printf("SceneRenderer::onRenderComponentRemoved");
    m_needsSortRenderableEntities = true;
    event.entity.removeComponent<EntityRenderState>();
}

ObjectDataUBO* SceneRenderer::mappedWorldTransformsBuffer(size_t maxObjects) {
    PROFILE_SCOPE("SceneRenderer::mappedWorldTransformsBuffer");

    vk::DeviceSize newBufferSize = sizeof(ObjectDataUBO) * maxObjects;

    if (m_resources->worldTransformBuffer == nullptr || newBufferSize > m_resources->worldTransformBuffer->getSize()) {
        PROFILE_SCOPE("Allocate WorldTransformBuffer");

        printf("Allocating WorldTransformBuffer - %llu objects\n", maxObjects);

        m_textureDescriptorIndices.clear();
        m_resources->materialBufferTextures.clear();
        m_resources->materialBufferImageLayouts.clear();

        m_resources->objectTextures.clear();
        m_resources->objectTextures.resize(maxObjects, NULL);

        m_resources->objectEntities.clear();
        m_resources->objectEntities.resize(maxObjects, entt::null);

        m_resources->objectBuffer.clear();
        m_resources->objectBuffer.resize(maxObjects, ObjectDataUBO{ .modelMatrix = glm::mat4(0.0), .textureIndex = 0 });


        BufferConfiguration worldTransformBufferConfig;
        worldTransformBufferConfig.device = Application::instance()->graphics()->getDevice();
        worldTransformBufferConfig.size = newBufferSize;
        worldTransformBufferConfig.memoryProperties =
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        worldTransformBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;
        m_resources->worldTransformBuffer = std::move(Buffer::create(worldTransformBufferConfig));


        DescriptorSetWriter(m_resources->objectDescriptorSet).writeBuffer(0, m_resources->worldTransformBuffer, 0, newBufferSize).write();
    }

    PROFILE_REGION("Map buffer");
    void* mappedBuffer = m_resources->worldTransformBuffer->map();
    return static_cast<ObjectDataUBO*>(mappedBuffer);
}

