
#ifndef WORLDENGINE_SCENERENDERER_H
#define WORLDENGINE_SCENERENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/RenderComponent.h"
#include "core/util/EntityChangeTracker.h"

class Buffer;
class DescriptorSet;
class Texture2D;
class Image2D;

struct GraphicsPipelineConfiguration;

struct CameraInfoUBO {
    glm::mat4 viewProjectionMatrix;
};

struct ObjectDataUBO {
    glm::mat4 modelMatrix;
    uint32_t textureIndex;
    uint32_t _pad0[3];
};


class SceneRenderer {
public:
    SceneRenderer();

    ~SceneRenderer();

    void init();

    void render(double dt);

    void render(double dt, RenderCamera* renderCamera);

    void setScene(Scene* scene);

    Scene* getScene() const;

    void initPipelineDescriptorSetLayouts(GraphicsPipelineConfiguration& graphicsPipelineConfiguration) const;

    uint32_t registerTexture(Texture2D* texture);

    void notifyMeshChanged(const RenderComponent::UpdateType& updateType);

    void notifyTransformChanged(const uint32_t& entityIndex);

    void notifyTextureChanged(const uint32_t& entityIndex);

//    template<typename T>
//    void setEntityUpdateType(const Entity& entity, const RenderComponent::UpdateType& updateType);

private:
    void recordRenderCommands(double dt, vk::CommandBuffer commandBuffer);

    void initMissingTexture();

    void sortRenderableEntities();

    void markChangedObjectTransforms(const size_t& rangeStart, const size_t& rangeEnd);

    void markChangedTextureIndices(const size_t& rangeStart, const size_t& rangeEnd);

    void findModifiedEntities();

    void updateEntityWorldTransforms();

    void updateMaterialsBuffer();

    void streamObjectData();

    void onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event);

    void onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event);

    ObjectDataUBO* mappedWorldTransformsBuffer(size_t maxObjects);

    void notifyEntityModified(const uint32_t& entityIndex);

    void recreateSwapchain(const RecreateSwapchainEvent& event);
private:
    Scene* m_scene;
    RenderCamera m_renderCamera;


    struct RenderResources {
        Buffer* cameraInfoBuffer;
        Buffer* worldTransformBuffer;
        DescriptorSet* globalDescriptorSet;
        DescriptorSet* objectDescriptorSet;
        DescriptorSet* materialDescriptorSet;
        DenseFlagArray changedObjectTransforms;
        DenseFlagArray changedObjectTextures;
        std::vector<ObjectDataUBO> objectBuffer;
        size_t uploadedMaterialBufferTextures;
        std::set<uint32_t> modifiedEntities;
    };

    FrameResource<RenderResources> m_resources;

    std::shared_ptr<DescriptorSetLayout> m_globalDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_objectDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_materialDescriptorSetLayout;

    std::shared_ptr<Image2D> m_missingTextureImage;
    std::shared_ptr<Texture2D> m_missingTexture;

    std::unordered_map<Texture2D*, uint32_t> m_textureDescriptorIndices;
    std::vector<Texture2D*> m_materialBufferTextures;
    std::vector<vk::ImageLayout> m_materialBufferImageLayouts;
    size_t m_numRenderEntities;


    std::unordered_map<RenderComponent::UpdateType, bool> m_needsSortEntities;
    std::vector<uint32_t> m_sortedModifiedEntities;
    std::vector<std::pair<size_t, size_t>> m_objectBufferModifiedRegions;

    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
};


#endif //WORLDENGINE_SCENERENDERER_H
