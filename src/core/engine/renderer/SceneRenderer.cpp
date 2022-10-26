#include "core/engine/renderer/SceneRenderer.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/engine/renderer/Material.h"
#include "core/engine/renderer/renderPasses/DeferredRenderer.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/scene/Transform.h"
#include "core/application/Engine.h"
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
    uint32_t instanceCount = 0;
    uint32_t firstInstance = 0;
};


template<typename T>
struct StaticUpdateFlag { uint8_t _v; };

template<typename T>
struct DynamicUpdateFlag { uint8_t _v; };

template<typename T>
struct AlwaysUpdateFlag { uint8_t _v; };

template<typename T>
struct UpdateTypeHandler {
    static void setEntityUpdateType(const Entity& entity, const RenderComponent::UpdateType& updateType) {
        switch (updateType) {
            case RenderComponent::UpdateType_Static: entity.addComponent<StaticUpdateFlag<T>>(); break;
            case RenderComponent::UpdateType_Dynamic: entity.addComponent<DynamicUpdateFlag<T>>(); break;
            case RenderComponent::UpdateType_Always: entity.addComponent<AlwaysUpdateFlag<T>>(); break;
            default: assert(false); break;
        }
    }

    static void removeEntityUpdateType(const Entity& entity, const RenderComponent::UpdateType& updateType) {
        switch (updateType) {
            case RenderComponent::UpdateType_Static: entity.removeComponent<StaticUpdateFlag<T>>(); break;
            case RenderComponent::UpdateType_Dynamic: entity.removeComponent<DynamicUpdateFlag<T>>(); break;
            case RenderComponent::UpdateType_Always: entity.removeComponent<AlwaysUpdateFlag<T>>(); break;
        }
    }
};





SceneRenderer::SceneRenderer():
//    m_needsSortRenderableEntities(true),
    m_numRenderEntities(0) {
}

SceneRenderer::~SceneRenderer() {
    m_missingTextureMaterial.reset();
    m_missingTextureImage.reset();
    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        delete m_resources[i]->worldTransformBuffer;
        delete m_resources[i]->materialDataBuffer;
        delete m_resources[i]->objectDescriptorSet;
        delete m_resources[i]->materialDescriptorSet;
    }
}

bool SceneRenderer::init() {
    initMissingTextureMaterial();

    std::shared_ptr<DescriptorPool> descriptorPool = Engine::graphics()->descriptorPool();

    DescriptorSetLayoutBuilder builder(descriptorPool->getDevice());

    constexpr size_t maxTextures = 0xFFFF;

    std::vector<Texture*> initialTextures;
    initialTextures.resize(maxTextures, m_missingTextureMaterial->getAlbedoMap().get());

    std::vector<vk::ImageLayout> initialImageLayouts;
    initialImageLayouts.resize(maxTextures, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_objectDescriptorSetLayout = builder
            .addStorageBuffer(0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .build("SceneRenderer-ObjectDescriptorSetLayout");

    m_materialDescriptorSetLayout = builder
            .addCombinedImageSampler(0, vk::ShaderStageFlagBits::eFragment, maxTextures)
            .addStorageBuffer(1, vk::ShaderStageFlagBits::eFragment)
            .build("SceneRenderer-MaterialDescriptorSetLayout");

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_resources.set(i, new RenderResources());
        m_resources[i]->objectDescriptorSet = DescriptorSet::create(m_objectDescriptorSetLayout, descriptorPool, "SceneRenderer-ObjectDescriptorSet");
        m_resources[i]->materialDescriptorSet = DescriptorSet::create(m_materialDescriptorSetLayout, descriptorPool, "SceneRenderer-MaterialDescriptorSet");

        m_resources[i]->worldTransformBuffer = nullptr;
        m_resources[i]->materialDataBuffer = nullptr;
        m_resources[i]->uploadedMaterialBufferTextures = 0;

        DescriptorSetWriter(m_resources[i]->materialDescriptorSet)
                .writeImage(0, initialTextures.data(), initialImageLayouts.data(), 0, maxTextures)
                .write();
    }

    m_scene->enableEvents<RenderComponent>();
    m_scene->getEventDispatcher()->connect<ComponentAddedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentAdded, this);
    m_scene->getEventDispatcher()->connect<ComponentRemovedEvent<RenderComponent>>(&SceneRenderer::onRenderComponentRemoved, this);

    m_needsSortEntities[RenderComponent::UpdateType_Static] = true;
    m_needsSortEntities[RenderComponent::UpdateType_Dynamic] = true;
    m_needsSortEntities[RenderComponent::UpdateType_Always] = true;

    return true;
}

void SceneRenderer::preRender(const double& dt) {
    PROFILE_SCOPE("SceneRenderer::preRender")
    const auto& renderEntities = m_scene->registry()->group<RenderComponent, Transform>();
    m_numRenderEntities = renderEntities.size();

    mappedWorldTransformsBuffer(m_numRenderEntities);
    mappedMaterialDataBuffer(m_numRenderEntities);

    sortRenderableEntities();
    findModifiedEntities();
    updateMaterialsBuffer();
    updateEntityWorldTransforms();
    streamObjectData();
}

void SceneRenderer::render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera) {
    PROFILE_SCOPE("SceneRenderer::render")
    PROFILE_BEGIN_GPU_CMD("SceneRenderer::render", commandBuffer);

    // TODO: add option for simplified rendering used for shadow maps/reflection probes.
    // TODO: frustum culling / occlusion culling based on provided camera

    recordRenderCommands(dt, commandBuffer);

    PROFILE_END_GPU_CMD(commandBuffer);
}

void SceneRenderer::setScene(Scene* scene) {
    m_scene = scene;
}

Scene* SceneRenderer::getScene() const {
    return m_scene;
}

const std::shared_ptr<DescriptorSetLayout>& SceneRenderer::getObjectDescriptorSetLayout() const {
    return m_objectDescriptorSetLayout;
}

const std::shared_ptr<DescriptorSetLayout>& SceneRenderer::getMaterialDescriptorSetLayout() const {
    return m_materialDescriptorSetLayout;
}

DescriptorSet* SceneRenderer::getObjectDescriptorSet() const {
    return m_resources->objectDescriptorSet;
}

DescriptorSet* SceneRenderer::getMaterialDescriptorSet() const {
    return m_resources->materialDescriptorSet;
}


void SceneRenderer::recordRenderCommands(const double& dt, const vk::CommandBuffer& commandBuffer) {
    PROFILE_SCOPE("SceneRenderer::recordRenderCommands");
    PROFILE_BEGIN_GPU_CMD("SceneRenderer::recordRenderCommands", commandBuffer);

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("SceneRenderer::recordRenderCommands/thread_exec");
        const auto& renderEntities = m_scene->registry()->group<RenderComponent, Transform>();

        std::vector<DrawCommand> drawCommands;

        DrawCommand currDrawCommand;
        currDrawCommand.firstInstance = (uint32_t)rangeStart;

        auto it = renderEntities.begin() + (uint32_t)rangeStart;
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

    if (m_numRenderEntities < 256) {
        PROFILE_REGION("Single-Threaded gather draw commands")
        const auto& drawCommands = thread_exec(0, m_numRenderEntities);

        PROFILE_REGION("Draw meshes")
        for (auto& command : drawCommands)
            command.mesh->draw(commandBuffer, command.instanceCount, command.firstInstance);

    } else {
        PROFILE_REGION("Multi-Threaded gather draw commands")
        auto futures = ThreadUtils::parallel_range(m_numRenderEntities, (size_t) 1,(size_t) ThreadUtils::getThreadCount(), thread_exec);
        auto drawCommands = ThreadUtils::getResults(futures);
        PROFILE_REGION("Draw meshes")
        for (auto& commands: drawCommands)
            for (auto& command: commands)
                command.mesh->draw(commandBuffer, command.instanceCount, command.firstInstance);
    }

    PROFILE_END_GPU_CMD(commandBuffer);
}

void SceneRenderer::initMissingTextureMaterial() {
    union {
        glm::u8vec4 pixels[2][2];
        uint8_t pixelBytes[sizeof(glm::u8vec4) * 4];
    };

    pixels[0][0] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);
    pixels[0][1] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][0] = glm::u8vec4(0x00, 0x00, 0x00, 0xFF);
    pixels[1][1] = glm::u8vec4(0xFF, 0x00, 0xFF, 0xFF);

    ImageData imageData(pixelBytes, 2, 2, ImagePixelLayout::RGBA, ImagePixelFormat::UInt8);

    Image2DConfiguration missingTextureImageConfig{};
    missingTextureImageConfig.device = Engine::graphics()->getDevice();
    missingTextureImageConfig.format = vk::Format::eR8G8B8A8Unorm;
    missingTextureImageConfig.imageData = &imageData;
    m_missingTextureImage = std::shared_ptr<Image2D>(Image2D::create(missingTextureImageConfig, "SceneRenderer-MissingTextureImage"));

    ImageViewConfiguration missingTextureImageViewConfig{};
    missingTextureImageViewConfig.device = missingTextureImageConfig.device;
    missingTextureImageViewConfig.format = missingTextureImageConfig.format;
    missingTextureImageViewConfig.setImage(m_missingTextureImage.get());

    SamplerConfiguration missingTextureSamplerConfig{};
    missingTextureSamplerConfig.device = missingTextureImageConfig.device;
    missingTextureSamplerConfig.minFilter = vk::Filter::eNearest;
    missingTextureSamplerConfig.magFilter = vk::Filter::eNearest;

    MaterialConfiguration materialConfig;
    materialConfig.setAlbedoMap(missingTextureImageViewConfig, missingTextureSamplerConfig, "SceneRenderer-MissingTexture");

    m_missingTextureMaterial = std::shared_ptr<Material>(Material::create(materialConfig));

    // Missing texture needs to be at index 0
    m_materialBufferTextures.clear();
    registerMaterial(m_missingTextureMaterial.get());
}

void SceneRenderer::sortRenderableEntities() {
    PROFILE_SCOPE("SceneRenderer::sortRenderableEntities")


    // Group all meshes of the same type to be contiguous when iterated
    auto renderComponentComp = [](const RenderComponent& lhs, const RenderComponent& rhs) {
        return lhs.mesh()->getResourceId() < rhs.mesh()->getResourceId();
    };

    bool entityRenderSequenceChanged = false;

    if (m_needsSortEntities[RenderComponent::UpdateType_Static]) {
        PROFILE_REGION("Sort entities UpdateType_Static")

        m_needsSortEntities[RenderComponent::UpdateType_Static] = false;

        const auto &grp = m_scene->registry()->group<RenderComponent, Transform, StaticUpdateFlag<Mesh>>();
        grp.sort([&grp, &comp = renderComponentComp](const entt::entity &lhs, const entt::entity &rhs) {
            return comp(grp.get<RenderComponent>(lhs), grp.get<RenderComponent>(rhs));
        }, entt::insertion_sort{});

        entityRenderSequenceChanged = true;
    }

    if (m_needsSortEntities[RenderComponent::UpdateType_Dynamic]) {
        PROFILE_REGION("Sort entities UpdateType_Dynamic")

        m_needsSortEntities[RenderComponent::UpdateType_Dynamic] = false;

        const auto &grp = m_scene->registry()->group<RenderComponent, Transform, DynamicUpdateFlag<Mesh>>();
        grp.sort([&grp, &comp = renderComponentComp](const entt::entity &lhs, const entt::entity &rhs) {
            return comp(grp.get<RenderComponent>(lhs), grp.get<RenderComponent>(rhs));
        }, entt::insertion_sort{});

        entityRenderSequenceChanged = true;
    }

    if (m_needsSortEntities[RenderComponent::UpdateType_Always]) {
        PROFILE_REGION("Sort entities UpdateType_Always")

        m_needsSortEntities[RenderComponent::UpdateType_Always] = false;

        const auto &grp = m_scene->registry()->group<RenderComponent, Transform, AlwaysUpdateFlag<Mesh>>();
        grp.sort([&grp, &comp = renderComponentComp](const entt::entity &lhs, const entt::entity &rhs) {
            return comp(grp.get<RenderComponent>(lhs), grp.get<RenderComponent>(rhs));
        }, entt::insertion_sort{});

        entityRenderSequenceChanged = true;
    }

    if (entityRenderSequenceChanged) {
        const auto &renderEntities = m_scene->registry()->group<RenderComponent, Transform>();

        EntityChangeTracker::entity_index index;

        PROFILE_REGION("Reindex transforms")
        index = 0;
        for (auto id : renderEntities) {
            Transform &transform = renderEntities.get<Transform>(id);
            Transform::reindex(transform, index);
            notifyTransformChanged(index);
            ++index;
        }

        PROFILE_REGION("Reindex render components")
        index = 0;
        for (auto id : renderEntities) {
            RenderComponent &renderComponent = renderEntities.get<RenderComponent>(id);
            RenderComponent::reindex(renderComponent, index);
            notifyMaterialChanged(index);
            ++index;
        }
    }
}

void SceneRenderer::markChangedObjectTransforms(const size_t& rangeStart, const size_t& rangeEnd) {
//    if (rangeEnd < m_firstChangedTransformIndex || rangeStart >= m_lastChangedTransformIndex)
//        return;
//
//    size_t changeTrackStartIndex = SIZE_MAX;
//
//    for (size_t index = rangeStart; index != rangeEnd; ++index) {
//
//        if (Transform::changeTracker().hasChanged(index)) {
//            if (changeTrackStartIndex == SIZE_MAX)
//                changeTrackStartIndex = index;
//
//        } else if (changeTrackStartIndex != SIZE_MAX) {
//            Transform::changeTracker().setChanged(changeTrackStartIndex, index - changeTrackStartIndex, false);
//            for (int i = 0; i < CONCURRENT_FRAMES; ++i)
//                m_resources.get(i)->changedObjectTransforms.set(changeTrackStartIndex, index - changeTrackStartIndex, true);
//            changeTrackStartIndex = SIZE_MAX;
//        }
//    }
//    if (changeTrackStartIndex != SIZE_MAX) {
//        Transform::changeTracker().setChanged(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, false);
//        for (int i = 0; i < CONCURRENT_FRAMES; ++i)
//            m_resources.get(i)->changedObjectTransforms.set(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, true);
//    }
}

void SceneRenderer::markChangedMaterialIndices(const size_t& rangeStart, const size_t& rangeEnd) {
//    size_t changeTrackStartIndex = SIZE_MAX;
//
//    for (size_t index = rangeStart; index != rangeEnd; ++index) {
//
//        if (RenderComponent::materialChangeTracker().hasChanged(index)) {
//            if (changeTrackStartIndex == SIZE_MAX)
//                changeTrackStartIndex = index;
//
//        } else if (changeTrackStartIndex != SIZE_MAX) {
//            RenderComponent::materialChangeTracker().setChanged(changeTrackStartIndex, index - changeTrackStartIndex, false);
//            for (int i = 0; i < CONCURRENT_FRAMES; ++i)
//                m_resources.get(i)->changedObjectMaterials.set(changeTrackStartIndex, index - changeTrackStartIndex, true);
//            changeTrackStartIndex = SIZE_MAX;
//        }
//    }
//
//    if (changeTrackStartIndex != SIZE_MAX) {
//        RenderComponent::materialChangeTracker().setChanged(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, false);
//        for (int i = 0; i < CONCURRENT_FRAMES; ++i)
//            m_resources.get(i)->changedObjectMaterials.set(changeTrackStartIndex, rangeEnd - changeTrackStartIndex, true);
//    }
}
//
//void SceneRenderer::findModifiedEntities() {
//    PROFILE_SCOPE("SceneRenderer::findModifiedEntities")
//
//    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
//        PROFILE_SCOPE("SceneRenderer::findModifiedEntities/thread_exec")
//
//        const auto& renderEntities = m_scene->registry()->group<RenderComponent, Transform>();
//
////        PROFILE_REGION("Mark changed object transforms");
////        markChangedObjectTransforms(rangeStart, rangeEnd);
//
////        PROFILE_REGION("Mark changed material indices");
////        markChangedMaterialIndices(rangeStart, rangeEnd);
//
//        PROFILE_REGION("Find modified regions")
//
//        std::vector<std::pair<size_t, size_t>> modifiedRegions;
//        size_t maxRegionMergeDist = 128;
//        size_t firstModifiedIndex = SIZE_MAX;
//        size_t lastModifiedIndex = SIZE_MAX;
//
//        for (size_t index = rangeStart; index != rangeEnd; ++index) {
//            if (m_resources->changedObjectTransforms[index] || m_resources->changedObjectMaterials[index]) {
//                if (firstModifiedIndex == SIZE_MAX) {
//                    firstModifiedIndex = index;
//
//                } else if (lastModifiedIndex + maxRegionMergeDist < index) {
//                    modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));
//                    firstModifiedIndex = index;
//                }
//                lastModifiedIndex = index;
//            }
//        }
//
//        if (firstModifiedIndex != SIZE_MAX)
//            modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));
//
//        return modifiedRegions;
//    };
//
//
//    PROFILE_REGION("Submit work")
//    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, DenseFlagArray::pack_bits, ThreadUtils::getThreadCount() * 5, thread_exec);
////    auto futures = ThreadUtils::run(thread_exec, 0, m_numRenderEntities);
//
//    PROFILE_REGION("Get thread results")
//    auto threadResults = ThreadUtils::getResults(futures); // Blocks until results are available.
//
//    PROFILE_REGION("Consolidate modified regions")
//
//    m_objectBufferModifiedRegions.clear();
//    for (auto&& regions : threadResults) {
//        m_objectBufferModifiedRegions.insert(m_objectBufferModifiedRegions.end(), regions.begin(), regions.end());
////        m_objectBufferModifiedRegions.emplace_back(regions);
//    }
//}

void SceneRenderer::findModifiedEntities() {
    PROFILE_SCOPE("SceneRenderer::findModifiedEntities")

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("SceneRenderer::findModifiedEntities/thread_exec")

        std::vector<std::pair<size_t, size_t>> modifiedRegions;
        size_t maxRegionMergeDist = 128;
        size_t firstModifiedIndex = SIZE_MAX;
        size_t lastModifiedIndex = SIZE_MAX;

        for (size_t index = rangeStart; index != rangeEnd; ++index) {
            const uint32_t& entityIndex = m_sortedModifiedEntities[index];
            if (firstModifiedIndex == SIZE_MAX) {
                firstModifiedIndex = entityIndex;

            } else if (lastModifiedIndex + maxRegionMergeDist < entityIndex) {
                modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));
                firstModifiedIndex = entityIndex;
            }
            lastModifiedIndex = entityIndex;
        }

        if (firstModifiedIndex != SIZE_MAX)
            modifiedRegions.emplace_back(std::make_pair(firstModifiedIndex, lastModifiedIndex + 1));

        return modifiedRegions;
    };

    PROFILE_REGION("Copy sorted modified entities")
    m_sortedModifiedEntities.resize(m_resources->modifiedEntities.size());
    std::copy(m_resources->modifiedEntities.begin(), m_resources->modifiedEntities.end(), m_sortedModifiedEntities.begin());
    m_resources->modifiedEntities.clear();

    PROFILE_REGION("Submit work")
    auto futures = ThreadUtils::parallel_range(m_sortedModifiedEntities.size(), thread_exec);

    PROFILE_REGION("Get thread results")
    auto threadResults = ThreadUtils::getResults(futures); // Blocks until results are available.

    PROFILE_REGION("Consolidate modified regions")

    m_objectBufferModifiedRegions.clear();
    for (auto&& regions : threadResults) {
        m_objectBufferModifiedRegions.insert(m_objectBufferModifiedRegions.end(), regions.begin(), regions.end());
//        m_objectBufferModifiedRegions.emplace_back(regions);
    }
}

void SceneRenderer::updateEntityWorldTransforms() {
    PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms")

    auto thread_exec = [this](size_t rangeStart,  size_t rangeEnd) {

        PROFILE_SCOPE("SceneRenderer::updateEntityWorldTransforms/thread_exec")

        const auto& renderEntities = m_scene->registry()->group<RenderComponent, Transform>();

        PROFILE_REGION("Update changed object transforms");

        bool anyTransformChanged = false;

        size_t itOffset = rangeStart;
        auto it = renderEntities.begin() + itOffset;

        PROFILE_REGION("Find first region for thread")
        size_t firstRegionIndex = 0;
        for (; firstRegionIndex < m_objectBufferModifiedRegions.size(); ++firstRegionIndex) {
            auto& region = m_objectBufferModifiedRegions[firstRegionIndex];
            if (region.second >= rangeStart)
                break;
        }

        PROFILE_REGION("Update transforms");
        for (size_t i = firstRegionIndex; i < m_objectBufferModifiedRegions.size(); ++i) {
            auto& region = m_objectBufferModifiedRegions[i];
            if (region.first >= rangeEnd)
                break; // Regions are sorted. Subsequent regions are also out of range.

            size_t regionStart = std::max(region.first, rangeStart);
            size_t regionEnd = std::min(region.second, rangeEnd);

            assert(regionStart >= itOffset);

            it += (regionStart - itOffset);
            itOffset = regionEnd;

            for (size_t index = regionStart; index != regionEnd; ++index, ++it) {
                if (m_resources->changedObjectTransforms[index]) {
                    glm::mat4& modelMatrix = m_resources->objectBuffer[index].modelMatrix;
                    m_resources->objectBuffer[index].prevModelMatrix = modelMatrix;
                    Transform& transform = renderEntities.get<Transform>(*it);
                    transform.fillMatrix(modelMatrix);
                    anyTransformChanged = true;
                }
                if (m_resources->recreatedWorldTransformBuffer) {
                    m_resources->objectBuffer[index].prevModelMatrix = m_resources->objectBuffer[index].modelMatrix;
                }
            }
        }

        if (anyTransformChanged)
            m_resources->changedObjectTransforms.set(rangeStart, rangeEnd - rangeStart, false);
    };

//    thread_exec(0, m_numRenderEntities);
    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, DenseFlagArray::pack_bits, ThreadUtils::getThreadCount() * 5, thread_exec);
//    auto futures = ThreadUtils::run(thread_exec, 0, m_numRenderEntities);

    ThreadUtils::wait(futures);
}

void SceneRenderer::updateMaterialsBuffer() {
    PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer")

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("SceneRenderer::updateMaterialsBuffer/thread_exec")

        const auto& renderEntities = m_scene->registry()->group<RenderComponent, Transform>();

        PROFILE_REGION("Update changed object materials");

        bool anyMaterialChanged = false;

        size_t itOffset = rangeStart;
        auto it = renderEntities.begin() + itOffset;

        PROFILE_REGION("Find first region for thread")
        size_t firstRegionIndex = 0;
        for (; firstRegionIndex < m_objectBufferModifiedRegions.size(); ++firstRegionIndex) {
            auto& region = m_objectBufferModifiedRegions[firstRegionIndex];
            if (region.second >= rangeStart)
                break;
        }

        PROFILE_REGION("Update texture indices");
        for (size_t i = firstRegionIndex; i < m_objectBufferModifiedRegions.size(); ++i) {
            auto& region = m_objectBufferModifiedRegions[i];
            if (region.first >= rangeEnd)
                break; // Regions are sorted. Subsequent regions are also out of range.

            size_t regionStart = std::max(region.first, rangeStart);
            size_t regionEnd = std::min(region.second, rangeEnd);

            assert(regionStart >= itOffset);

            it += (int64_t)regionStart - (int64_t)itOffset;
            itOffset = regionEnd;

            for (size_t index = regionStart; index != regionEnd; ++index, ++it) {
                if (m_resources->changedObjectMaterials[index]) {
                    const RenderComponent& renderComponent = renderEntities.get<RenderComponent>(*it);
                    uint32_t materialIndex = renderComponent.getMaterialIndex();
                    if (materialIndex > m_materials.size())
                        materialIndex = 0;

                    m_resources->materialBuffer[index] = m_materials[materialIndex];
                    anyMaterialChanged = true;
                }
            }
        }

        if (anyMaterialChanged)
            m_resources->changedObjectMaterials.set(rangeStart, rangeEnd - rangeStart, false);
    };

//    auto futures = ThreadUtils::run(thread_exec, 0, m_numRenderEntities);
    auto futures = ThreadUtils::parallel_range(m_numRenderEntities, DenseFlagArray::pack_bits, ThreadUtils::getThreadCount() * 5, thread_exec);
    ThreadUtils::wait(futures);

    uint32_t firstNewTextureIndex = m_resources->uploadedMaterialBufferTextures;
    uint32_t lastNewTextureIndex = (uint32_t)m_materialBufferTextures.size();

    uint32_t descriptorCount = m_resources->materialDescriptorSet->getLayout()->findBinding(0).descriptorCount;
    uint32_t arrayCount = glm::min((uint32_t)(lastNewTextureIndex - firstNewTextureIndex), descriptorCount);
    if (arrayCount > 0) {
        PROFILE_SCOPE("Write texture descriptors");

        printf("Writing textures [%llu to %llu]\n", (size_t)firstNewTextureIndex, (size_t)lastNewTextureIndex);
        DescriptorSetWriter(m_resources->materialDescriptorSet)
                .writeImage(0, &m_materialBufferTextures[firstNewTextureIndex], &m_materialBufferImageLayouts[firstNewTextureIndex], firstNewTextureIndex, arrayCount)
                .write();
    }

    m_resources->uploadedMaterialBufferTextures = (uint32_t)m_materialBufferTextures.size();
}

void SceneRenderer::streamObjectData() {
    PROFILE_SCOPE("SceneRenderer::streamObjectData")

    constexpr bool multithread = false;

    auto thread_exec = [this](size_t rangeStart, size_t rangeEnd) {
        PROFILE_SCOPE("Copy modified region");

        PROFILE_REGION("Copy object data");

        ObjectDataUBO* objectBufferMap = mappedWorldTransformsBuffer(m_numRenderEntities);

        for (size_t i = rangeStart; i != rangeEnd; ++i) {
            const std::pair<size_t, size_t>& region = m_objectBufferModifiedRegions[i];
            const size_t& start = region.first;
            const size_t& end = region.second;
            memcpy(&objectBufferMap[start], &m_resources->objectBuffer[start], (end - start) * sizeof(ObjectDataUBO));
        }

        PROFILE_REGION("Copy material data");

        GPUMaterial* materialBufferMap = mappedMaterialDataBuffer(m_numRenderEntities);

        for (size_t i = rangeStart; i != rangeEnd; ++i) {
            const std::pair<size_t, size_t>& region = m_objectBufferModifiedRegions[i];
            const size_t& start = region.first;
            const size_t& end = region.second;
            memcpy(&materialBufferMap[start], &m_resources->materialBuffer[start], (end - start) * sizeof(GPUMaterial));
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

uint32_t SceneRenderer::registerTexture(Texture* texture) {
    uint32_t textureIndex;

    auto it = m_textureDescriptorIndices.find(texture);
    if (it == m_textureDescriptorIndices.end()) {
        textureIndex = (uint32_t)m_materialBufferTextures.size();
        m_materialBufferTextures.emplace_back(texture);
        m_materialBufferImageLayouts.emplace_back(vk::ImageLayout::eShaderReadOnlyOptimal);

        m_textureDescriptorIndices.insert(std::make_pair(texture, textureIndex));
    } else {
        textureIndex = it->second;
    }

    return textureIndex;
}

uint32_t SceneRenderer::registerMaterial(Material* material) {
    uint32_t materialIndex;

//    m_materialBufferTextures.emplace_back(m_missingTexture.get());
//    m_materialBufferImageLayouts.emplace_back(vk::ImageLayout::eShaderReadOnlyOptimal);

    if (material == nullptr) {
        material = m_missingTextureMaterial.get();
        return 0; // Missing texture is expected to be at index 0
    }

    auto it = m_materialIndices.find(material);
    if (it == m_materialIndices.end()) {
        materialIndex = (uint32_t)m_materials.size();

        GPUMaterial gpuMaterial{};
        gpuMaterial.hasAlbedoTexture = material->hasAlbedoMap();
        gpuMaterial.hasRoughnessTexture = material->hasRoughnessMap();
        gpuMaterial.hasMetallicTexture = material->hasMetallicMap();
        gpuMaterial.hasEmissionTexture = material->hasEmissionMap();
        gpuMaterial.hasNormalTexture = material->hasNormalMap();
        if (gpuMaterial.hasAlbedoTexture) gpuMaterial.albedoTextureIndex = registerTexture(material->getAlbedoMap().get());
        if (gpuMaterial.hasRoughnessTexture) gpuMaterial.roughnessTextureIndex = registerTexture(material->getRoughnessMap().get());
        if (gpuMaterial.hasMetallicTexture) gpuMaterial.metallicTextureIndex = registerTexture(material->getMetallicMap().get());
        if (gpuMaterial.hasEmissionTexture) gpuMaterial.emissionTextureIndex = registerTexture(material->getEmissionMap().get());
        if (gpuMaterial.hasNormalTexture) gpuMaterial.normalTextureIndex = registerTexture(material->getNormalMap().get());
        gpuMaterial.albedoColour_r = material->getAlbedo().r;
        gpuMaterial.albedoColour_g = material->getAlbedo().g;
        gpuMaterial.albedoColour_b = material->getAlbedo().b;
        gpuMaterial.emission_r = material->getEmission().r;
        gpuMaterial.emission_g = material->getEmission().g;
        gpuMaterial.emission_b = material->getEmission().b;
        gpuMaterial.roughness = material->getRoughness();
        gpuMaterial.metallic = material->getMetallic();
        m_materials.emplace_back(gpuMaterial);

        m_materialIndices.insert(std::make_pair(material, materialIndex));
    } else {
        materialIndex = it->second;
    }
    return materialIndex;
}

void SceneRenderer::notifyMeshChanged(const RenderComponent::UpdateType& updateType) {
    m_needsSortEntities[updateType] = true;
}

void SceneRenderer::notifyTransformChanged(const uint32_t& entityIndex) {
    if (entityIndex == EntityChangeTracker::INVALID_INDEX)
        return;

    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        m_resources.get(i)->changedObjectTransforms.set(entityIndex, true);

    notifyEntityModified(entityIndex);
}

void SceneRenderer::notifyMaterialChanged(const uint32_t &entityIndex) {
    if (entityIndex == EntityChangeTracker::INVALID_INDEX)
        return;

    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        m_resources.get(i)->changedObjectMaterials.set(entityIndex, true);

    notifyEntityModified(entityIndex);
}

void SceneRenderer::notifyEntityModified(const uint32_t& entityIndex) {
    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        m_resources.get(i)->modifiedEntities.insert(entityIndex);
}

void SceneRenderer::onRenderComponentAdded(ComponentAddedEvent<RenderComponent>* event) {
    notifyMeshChanged(event->component->meshUpdateType());

    UpdateTypeHandler<Transform>::setEntityUpdateType(event->entity, event->component->transformUpdateType());
    UpdateTypeHandler<Texture>::setEntityUpdateType(event->entity, event->component->materialUpdateType());
    UpdateTypeHandler<Mesh>::setEntityUpdateType(event->entity, event->component->meshUpdateType());
}

void SceneRenderer::onRenderComponentRemoved(ComponentRemovedEvent<RenderComponent>* event) {
    notifyMeshChanged(event->component->meshUpdateType());

    UpdateTypeHandler<Transform>::removeEntityUpdateType(event->entity, event->component->transformUpdateType());
    UpdateTypeHandler<Texture>::removeEntityUpdateType(event->entity, event->component->materialUpdateType());
    UpdateTypeHandler<Mesh>::removeEntityUpdateType(event->entity, event->component->meshUpdateType());
}

ObjectDataUBO* SceneRenderer::mappedWorldTransformsBuffer(size_t maxObjects) {
    PROFILE_SCOPE("SceneRenderer::mappedWorldTransformsBuffer");

    vk::DeviceSize newBufferSize = sizeof(ObjectDataUBO) * maxObjects;

    if (m_resources->worldTransformBuffer == nullptr || newBufferSize > m_resources->worldTransformBuffer->getSize()) {
        PROFILE_SCOPE("Allocate WorldTransformBuffer");

        printf("Allocating WorldTransformBuffer - %llu objects\n", maxObjects);

        m_textureDescriptorIndices.clear();

        m_resources->changedObjectTransforms.clear();
        m_resources->changedObjectTransforms.resize(maxObjects, true);


        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
            m_resources.get(i)->changedObjectTransforms.ensureSize(maxObjects, true);
            m_resources.get(i)->changedObjectMaterials.ensureSize(maxObjects, true);
        }

        ObjectDataUBO defaultObjectData{};
        defaultObjectData.prevModelMatrix = glm::mat4(0.0);
        defaultObjectData.modelMatrix = glm::mat4(0.0);

        m_resources->objectBuffer.clear();
        m_resources->objectBuffer.resize(maxObjects, defaultObjectData);


        BufferConfiguration worldTransformBufferConfig{};
        worldTransformBufferConfig.device = Engine::graphics()->getDevice();
        worldTransformBufferConfig.size = newBufferSize;
        worldTransformBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        worldTransformBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_resources->worldTransformBuffer;
        m_resources->worldTransformBuffer = Buffer::create(worldTransformBufferConfig, "SceneRenderer-WorldTransformBuffer");


        DescriptorSetWriter(m_resources->objectDescriptorSet)
            .writeBuffer(0, m_resources->worldTransformBuffer, 0, newBufferSize)
            .write();

        m_resources->recreatedWorldTransformBuffer = true;
    } else {
        m_resources->recreatedWorldTransformBuffer = false;
    }

    PROFILE_REGION("Map buffer");
    void* mappedBuffer = m_resources->worldTransformBuffer->map();
    return static_cast<ObjectDataUBO*>(mappedBuffer);
}

GPUMaterial* SceneRenderer::mappedMaterialDataBuffer(size_t maxObjects) {
    PROFILE_SCOPE("SceneRenderer::mappedMaterialDataBuffer");

    vk::DeviceSize newBufferSize = sizeof(GPUMaterial) * maxObjects;

    if (m_resources->materialDataBuffer == nullptr || newBufferSize > m_resources->materialDataBuffer->getSize()) {
        PROFILE_SCOPE("Allocate materialDataBuffer");

        printf("Allocating materialDataBuffer - %llu objects\n", maxObjects);

        m_materialIndices.clear();

        m_resources->changedObjectMaterials.clear();
        m_resources->changedObjectMaterials.resize(maxObjects, true);


        for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
            m_resources.get(i)->changedObjectTransforms.ensureSize(maxObjects, true);
            m_resources.get(i)->changedObjectMaterials.ensureSize(maxObjects, true);
        }

        GPUMaterial defaultMaterial{};
        defaultMaterial.packedFlags = 0;
        defaultMaterial.packedAlbedoColour = 0;
        defaultMaterial.packedRoughnessMetallicEmissionR = 0;
        defaultMaterial.packedEmissionGB = 0;
        m_resources->materialBuffer.clear();
        m_resources->materialBuffer.resize(maxObjects, defaultMaterial);


        BufferConfiguration materialDataBufferConfig{};
        materialDataBufferConfig.device = Engine::graphics()->getDevice();
        materialDataBufferConfig.size = newBufferSize;
        materialDataBufferConfig.memoryProperties =
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        //vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        materialDataBufferConfig.usage = vk::BufferUsageFlagBits::eStorageBuffer;// | vk::BufferUsageFlagBits::eTransferDst;

        delete m_resources->materialDataBuffer;
        m_resources->materialDataBuffer = Buffer::create(materialDataBufferConfig, "SceneRenderer-MaterialDataBuffer");


        DescriptorSetWriter(m_resources->materialDescriptorSet)
            .writeBuffer(1, m_resources->materialDataBuffer, 0, newBufferSize)
            .write();
    }

    PROFILE_REGION("Map buffer");
    void* mappedBuffer = m_resources->materialDataBuffer->map();
    return static_cast<GPUMaterial*>(mappedBuffer);
}


