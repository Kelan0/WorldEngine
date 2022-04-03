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


struct DrawCommand {
    Mesh* mesh = nullptr;
    size_t instanceCount = 0;
    size_t firstInstance = 0;
};



SceneRenderer::SceneRenderer():
    m_needsSortRenderableEntities(true),
    m_numRenderEntities(0) {
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

        m_resources[i]->worldTransformBuffer = nullptr;
        m_resources[i]->uploadedMaterialBufferTextures = 0;

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

    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);
    m_numRenderEntities = renderEntities.size();

    if (m_needsSortRenderableEntities) {
        m_needsSortRenderableEntities = false;
        sortRenderableEntities();
    }

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

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("SceneRenderer::recordRenderCommands/thread_exec")
        const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

        std::vector<DrawCommand> drawCommands;

        DrawCommand currDrawCommand;
        currDrawCommand.firstInstance = rangeStart;

        auto it = renderEntities.begin() + rangeStart;
        for (size_t index = rangeStart; index != rangeEnd; ++index, ++it) {
            const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);

            if (currDrawCommand.mesh == nullptr) {
                currDrawCommand.mesh = renderComponent.mesh().get();
                currDrawCommand.instanceCount = 0;

            } else if (currDrawCommand.mesh != renderComponent.mesh().get()) {
                drawCommands.emplace_back(currDrawCommand);
                currDrawCommand.mesh = renderComponent.mesh().get();
                currDrawCommand.firstInstance += currDrawCommand.instanceCount;
                currDrawCommand.instanceCount = 0;
            }

            ++currDrawCommand.instanceCount;
        }
        drawCommands.emplace_back(currDrawCommand);

        return drawCommands;
    };

//    std::vector<DrawCommand> drawCommands = thread_exec(0, m_numRenderEntities);

    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, (size_t)1, ThreadUtils::getThreadCount(), thread_exec);
    auto drawCommands = ThreadUtils::getResults(futures);

    PROFILE_REGION("Bind resources")
    GraphicsManager* graphics = Application::instance()->graphics();
    const uint32_t currentFrameIndex = graphics->getCurrentFrameIndex();
    const vk::Device& device = **graphics->getDevice();



    std::shared_ptr<GraphicsPipeline> pipeline = Application::instance()->graphics()->pipeline();


    std::vector<vk::DescriptorSet> descriptorSets = {
            m_resources->globalDescriptorSet->getDescriptorSet(),
            m_resources->objectDescriptorSet->getDescriptorSet(),
            m_resources->materialDescriptorSet->getDescriptorSet(),
    };

    std::vector<uint32_t> dynamicOffsets = {};

    graphics->pipeline()->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics->pipeline()->getPipelineLayout(), 0, descriptorSets, dynamicOffsets);


    PROFILE_REGION("Draw meshes")
    for (auto& commands : drawCommands)
        for (auto& command : commands)
            command.mesh->draw(commandBuffer, command.instanceCount, command.firstInstance);

//    for (size_t i = 0; i < drawCommands.size(); ++i)
//        drawCommands[i].mesh->draw(commandBuffer, drawCommands[i].instanceCount, drawCommands[i].firstInstance);
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

    PROFILE_REGION("Sort entities")

    // Group all meshes of the same type to be contiguous in the array
    renderEntities.sort([&renderEntities](const entt::entity& lhs, const entt::entity& rhs) {
        const RenderComponent& lhsRC = renderEntities.get<RenderComponent>(lhs);
        const RenderComponent& rhsRC = renderEntities.get<RenderComponent>(rhs);
        return lhsRC.mesh()->getResourceId() < rhsRC.mesh()->getResourceId();
    });

    PROFILE_REGION("Reindex transforms")
    Transform::changeTracker().ensureCapacity(m_numRenderEntities);
    EntityChangeTracker::entity_index index = 0;
    for (auto id : renderEntities) {
        Transform& transform = renderEntities.get<Transform>(id);
        Transform::reindex(transform, index++);
    }


    PROFILE_REGION("Reindex render components")
    index = 0;
    RenderComponent::textureChangeTracker().ensureCapacity(m_numRenderEntities);
    for (auto id : renderEntities) {
        RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);
        RenderComponent::reindex(renderComponent, index++);
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

    PROFILE_REGION("Ensure changedObjectTransforms capacity")
    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        m_resources.get(i)->changedObjectTransforms.ensureSize(m_numRenderEntities, true);

    auto thread_exec = [this](
            size_t rangeStart,
            size_t rangeEnd) {

        PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms/thread_exec")

        const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

        std::vector<std::pair<size_t, size_t>> modifiedRegions;

        size_t maxRegionMergeDist = 128;
        size_t firstModifiedIndex = SIZE_MAX;
        size_t lastModifiedIndex = SIZE_MAX;

        PROFILE_REGION("Mark changed object transforms");

        size_t changeTrackStartIndex = SIZE_MAX;

        for (size_t index = rangeStart; index != rangeEnd; ++index) {

            if (Transform::changeTracker().hasChanged(index)) {
                if (changeTrackStartIndex == SIZE_MAX)
                    changeTrackStartIndex = index;

            } else if (changeTrackStartIndex != SIZE_MAX) {
                Transform::changeTracker().setChanged(changeTrackStartIndex, index - changeTrackStartIndex, false);
                for (int i = 0; i < CONCURRENT_FRAMES; ++i)
                    m_resources.get(i)->changedObjectTransforms.set(changeTrackStartIndex, index - changeTrackStartIndex, true);

                changeTrackStartIndex = SIZE_MAX;
            }
        }

        if (changeTrackStartIndex != SIZE_MAX) {
            Transform::changeTracker().setChanged(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, false);
            for (int i = 0; i < CONCURRENT_FRAMES; ++i)
                m_resources.get(i)->changedObjectTransforms.set(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, true);
        }

        PROFILE_REGION("Find modified regions")

        for (size_t index = rangeStart; index != rangeEnd; ++index) {
            if (m_resources->changedObjectTransforms[index]) {
                if (firstModifiedIndex == SIZE_MAX) {
                    firstModifiedIndex = index;

                } else if (lastModifiedIndex + maxRegionMergeDist < index) {
                    modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));
                    firstModifiedIndex = index;
                }
                lastModifiedIndex = index;
            }
        }

        if (firstModifiedIndex != SIZE_MAX)
            modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));

        PROFILE_REGION("Update changed object transforms");

        size_t itOffset = rangeStart;
        auto it = renderEntities.begin() + itOffset;

        for (auto& region : modifiedRegions) {
            assert(region.first >= itOffset);

            it += (region.first - itOffset);
            itOffset = region.second;

            for (size_t index = region.first; index != region.second; ++index, ++it) {
                if (m_resources->changedObjectTransforms[index]) {
                    glm::mat4& modelMatrix = m_resources->objectBuffer[index].modelMatrix;
                    Transform& transform = renderEntities.get<Transform>(*it);
                    transform.fillMatrix(modelMatrix);
                }
            }
        }

        return modifiedRegions;
    };

    PROFILE_REGION("Submit work")
//    thread_exec(0, m_numRenderEntities);
    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, DenseFlagArray::pack_bits, ThreadUtils::getThreadCount() * 5, thread_exec);
//    auto futures = ThreadUtils::run(thread_exec, 0, m_numRenderEntities);

    PROFILE_REGION("Get thread results")
    auto threadResults = ThreadUtils::getResults(futures); // Blocks until results are available.

    PROFILE_REGION("Reset modified transform flags")
    
    m_resources->changedObjectTransforms.set(0, m_resources->changedObjectTransforms.size(), false);

    PROFILE_REGION("Consolidate modified regions")

    m_objectBufferModifiedRegions.clear();
    for (auto&& regions : threadResults) {
        m_objectBufferModifiedRegions.insert(m_objectBufferModifiedRegions.end(), regions.begin(), regions.end());
//        m_objectBufferModifiedRegions.emplace_back(regions);
    }
}

void SceneRenderer::streamObjectData() {
    PROFILE_SCOPE("SceneRenderer::streamObjectData")

    constexpr bool multithread = false;

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("Copy modified region");

        ObjectDataUBO* objectBufferMap = mappedWorldTransformsBuffer(m_numRenderEntities);

        for (size_t i = rangeStart; i != rangeEnd; ++i) {
            const std::pair<size_t, size_t>& region = m_objectBufferModifiedRegions[i];
            const size_t& start = region.first;
            const size_t& end = region.second;
            memcpy(&objectBufferMap[start], &m_resources->objectBuffer[start], (end - start) * sizeof(ObjectDataUBO));
        }
    };


    if (multithread) {
        // TODO: this is faster for large amounts of data, but slower for small amounts of data. Tune this.
        auto futures = ThreadUtils::parallel_range(m_objectBufferModifiedRegions.size(), thread_exec);
        ThreadUtils::wait(futures);
    } else {
        thread_exec(0, m_objectBufferModifiedRegions.size());
    }


    //memcpy(&objectBufferMap[0], &m_resources->objectBuffer[0], m_resources->objectBuffer.size() * sizeof(ObjectDataUBO));
}

void SceneRenderer::updateMaterialsBuffer() {
    PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer")


    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        m_resources.get(i)->changedObjectTextures.ensureSize(m_numRenderEntities, true);

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer/thread_exec")

        const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);

        PROFILE_REGION("Mark changed texture indices");

        size_t changeTrackStartIndex = SIZE_MAX;

        for (size_t index = rangeStart; index != rangeEnd; ++index) {

            if (RenderComponent::textureChangeTracker().hasChanged(index)) {
                if (changeTrackStartIndex == SIZE_MAX)
                    changeTrackStartIndex = index;

            } else if (changeTrackStartIndex != SIZE_MAX) {
                RenderComponent::textureChangeTracker().setChanged(changeTrackStartIndex, index - changeTrackStartIndex, false);
                for (int i = 0; i < CONCURRENT_FRAMES; ++i)
                    m_resources.get(i)->changedObjectTextures.set(changeTrackStartIndex, index - changeTrackStartIndex, true);

                changeTrackStartIndex = SIZE_MAX;
            }
        }

        if (changeTrackStartIndex != SIZE_MAX) {
            RenderComponent::textureChangeTracker().setChanged(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, false);
            for (int i = 0; i < CONCURRENT_FRAMES; ++i)
                m_resources.get(i)->changedObjectTextures.set(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, true);
        }

        PROFILE_REGION("Find modified regions")

        std::vector<std::pair<size_t, size_t>> modifiedRegions;
        size_t firstModifiedIndex = SIZE_MAX;
        size_t lastModifiedIndex = SIZE_MAX;
        size_t maxRegionMergeDist = 128;

        for (size_t index = rangeStart; index != rangeEnd; ++index) {
            if (m_resources->changedObjectTextures[index]) {
                if (firstModifiedIndex == SIZE_MAX) {
                    firstModifiedIndex = index;

                } else if (lastModifiedIndex + maxRegionMergeDist < index) {
                    modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));
                    firstModifiedIndex = index;
                }
                lastModifiedIndex = index;
            }
        }

        if (firstModifiedIndex != SIZE_MAX)
            modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));

        PROFILE_REGION("Update changed object textures");

        bool anyTextureChanged = false;

        size_t itOffset = rangeStart;
        auto it = renderEntities.begin() + itOffset;

        for (auto& region : modifiedRegions) {
            assert(region.first >= itOffset);

            it += (region.first - itOffset);
            itOffset = region.second;

            for (size_t index = region.first; index != region.second; ++index, ++it) {
                if (m_resources->changedObjectTextures[index]) {
                    const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);
                    m_resources->objectBuffer[index].textureIndex = renderComponent.getTextureIndex();
                    anyTextureChanged = true;
                }
            }
        }

        if (anyTextureChanged)
            m_resources->changedObjectTextures.set(rangeStart, rangeEnd - rangeStart, false);
    };

    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, DenseFlagArray::pack_bits, ThreadUtils::getThreadCount() * 5, thread_exec);
    ThreadUtils::wait(futures);

    size_t firstNewTextureIndex = m_resources->uploadedMaterialBufferTextures;
    size_t lastNewTextureIndex = m_materialBufferTextures.size();

    size_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;
    size_t arrayCount = glm::min(lastNewTextureIndex - firstNewTextureIndex, descriptorCount);
    if (arrayCount > 0) {
        PROFILE_SCOPE("Write texture descriptors");

        printf("Writing textures [%d to %d]\n", firstNewTextureIndex, lastNewTextureIndex);
        DescriptorSetWriter(m_resources->materialDescriptorSet)
                .writeImage(0, &m_materialBufferTextures[firstNewTextureIndex], &m_materialBufferImageLayouts[firstNewTextureIndex], arrayCount, firstNewTextureIndex)
                .write();
    }

    m_resources->uploadedMaterialBufferTextures = m_materialBufferTextures.size();
}

//void SceneRenderer::updateMaterialsBuffer() {
//    PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer")
//
//    uint32_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;
//
//    const auto& renderEntities = m_scene->registry()->group<RenderComponent>(entt::get<Transform>);
//
//    //ObjectDataUBO* objectDataBuffer = mappedWorldTransformsBuffer(renderEntities.size());
//
//    std::vector<Texture2D*>& objectTextures = m_resources->objectTextures;
//    std::vector<Texture2D*>& materialBufferTextures = m_resources->materialBufferTextures;
//
//    uint32_t textureIndex;
//    size_t objectIndex = 0;
//    uint32_t firstNewTextureIndex = (uint32_t)(-1);
//    uint32_t lastNewTextureIndex = (uint32_t)(-1);
//    Texture2D* texture;
//
//    size_t numChangedTextures = 0;
//
//    for (auto id : renderEntities) {
//        const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(id);
//
//        texture = renderComponent.texture().get();
//        if (texture == NULL)
//            texture = m_missingTexture.get();
//
//        if (objectTextures[objectIndex] != texture) {
//            objectTextures[objectIndex] = texture;
//
//            ++numChangedTextures;
//
//            auto it = m_textureDescriptorIndices.find(texture);
//            if (it == m_textureDescriptorIndices.end()) {
//                textureIndex = materialBufferTextures.size();
//                materialBufferTextures.emplace_back(texture);
//
//                lastNewTextureIndex = textureIndex;
//                if (firstNewTextureIndex == (uint32_t)(-1))
//                    firstNewTextureIndex = textureIndex;
//
//                m_textureDescriptorIndices.insert(std::make_pair(texture, textureIndex));
//            } else {
//                textureIndex = it->second;
//            }
//
//            m_resources->objectBuffer[objectIndex].textureIndex = textureIndex;
//        }
//
//        ++objectIndex;
//    }
//
//    //auto t1 = Performance::now();
//    //printf("Took %.4f msec to update %llu changed textures in %llu entities\n", Performance::milliseconds(t0, t1), numChangedTextures, renderEntities.size());
//
//    uint32_t arrayCount = glm::min(lastNewTextureIndex - firstNewTextureIndex, descriptorCount);
//    if (arrayCount > 0) {
//
//        std::vector<vk::ImageLayout>& materialBufferImageLayouts = m_resources->materialBufferImageLayouts;
//        if (materialBufferImageLayouts.size() < arrayCount)
//            materialBufferImageLayouts.resize(arrayCount, vk::ImageLayout::eShaderReadOnlyOptimal);
//
//        printf("Writing textures [%d to %d]\n", firstNewTextureIndex, lastNewTextureIndex);
//        DescriptorSetWriter(m_resources->materialDescriptorSet)
//                .writeImage(0, &materialBufferTextures[firstNewTextureIndex], &materialBufferImageLayouts[0], arrayCount, firstNewTextureIndex)
//                .write();
//    }
//}

uint32_t SceneRenderer::registerTexture(Texture2D* texture) {
    uint32_t textureIndex;

    if (texture == nullptr)
        texture = m_missingTexture.get();

    auto it = m_textureDescriptorIndices.find(texture);
    if (it == m_textureDescriptorIndices.end()) {
        textureIndex = m_materialBufferTextures.size();
        m_materialBufferTextures.emplace_back(texture);
        m_materialBufferImageLayouts.emplace_back(vk::ImageLayout::eShaderReadOnlyOptimal);

        m_textureDescriptorIndices.insert(std::make_pair(texture, textureIndex));
    } else {
        textureIndex = it->second;
    }
    return textureIndex;
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

// //        m_materialBufferTextures.clear();
// //        m_materialBufferImageLayouts.clear();

//        m_resources->materialBufferTextures.clear();
//        m_resources->materialBufferImageLayouts.clear();
//
//        m_resources->objectTextures.clear();
//        m_resources->objectTextures.resize(maxObjects, NULL);

        m_resources->changedObjectTransforms.clear();
        m_resources->changedObjectTransforms.resize(maxObjects, true);

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


