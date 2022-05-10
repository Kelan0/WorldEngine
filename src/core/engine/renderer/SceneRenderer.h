
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
class DescriptorSetLayout;
class Texture;
class Material;
class Image2D;

struct GraphicsPipelineConfiguration;

struct ObjectDataUBO {
    glm::mat4 modelMatrix;
};

struct GPUMaterial {
    uint32_t albedoTextureIndex;
    uint32_t roughnessTextureIndex;
    uint32_t metallicTextureIndex;
    uint32_t normalTextureIndex;
    union {
        uint32_t packedAlbedoColour; // XRGB
        struct {
            uint8_t albedoColour_b;
            uint8_t albedoColour_g;
            uint8_t albedoColour_r;
            uint8_t _unused0;
        };
    };
    union {
        uint32_t packedRoughnessMetallic; // XXRM
        struct {
            uint8_t metallic;
            uint8_t roughness;
            uint8_t _unused1;
            uint8_t _unused2;
        };
    };

    union {
        uint32_t packedFlags;
        struct {
            bool hasAlbedoTexture : 1;
            bool hasRoughnessTexture : 1;
            bool hasMetallicTexture : 1;
            bool hasNormalTexture : 1;
        };
    };
    uint32_t _pad0;
};

class SceneRenderer {
public:
    SceneRenderer();

    ~SceneRenderer();

    bool init();

    void preRender(double dt);

    void render(double dt);

    void render(double dt, RenderCamera* renderCamera);

    void setScene(Scene* scene);

    Scene* getScene() const;

    void initPipelineDescriptorSetLayouts(GraphicsPipelineConfiguration& graphicsPipelineConfiguration) const;

    uint32_t registerTexture(Texture* texture);

    uint32_t registerMaterial(Material* material);

    void notifyMeshChanged(const RenderComponent::UpdateType& updateType);

    void notifyTransformChanged(const uint32_t& entityIndex);

    void notifyMaterialChanged(const uint32_t& entityIndex);

private:
    void recordRenderCommands(double dt, const vk::CommandBuffer& commandBuffer);

    void initMissingTextureMaterial();

    void sortRenderableEntities();

    void markChangedObjectTransforms(const size_t& rangeStart, const size_t& rangeEnd);

    void markChangedMaterialIndices(const size_t& rangeStart, const size_t& rangeEnd);

    void findModifiedEntities();

    void updateEntityWorldTransforms();

    void updateMaterialsBuffer();

    void streamObjectData();

    void onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event);

    void onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event);

    ObjectDataUBO* mappedWorldTransformsBuffer(size_t maxObjects);

    GPUMaterial* mappedMaterialDataBuffer(size_t maxObjects);

    void notifyEntityModified(const uint32_t& entityIndex);

private:
    Scene* m_scene;
    RenderCamera m_renderCamera;


    struct RenderResources {
        Buffer* cameraInfoBuffer;
        Buffer* worldTransformBuffer;
        Buffer* materialDataBuffer;
        DescriptorSet* globalDescriptorSet;
        DescriptorSet* objectDescriptorSet;
        DescriptorSet* materialDescriptorSet;
        DenseFlagArray changedObjectTransforms;
        DenseFlagArray changedObjectMaterials;
        std::vector<ObjectDataUBO> objectBuffer;
        std::vector<GPUMaterial> materialBuffer;
        size_t uploadedMaterialBufferTextures;
        std::set<uint32_t> modifiedEntities;
    };

    FrameResource<RenderResources> m_resources;

    std::shared_ptr<DescriptorSetLayout> m_globalDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_objectDescriptorSetLayout;
    std::shared_ptr<DescriptorSetLayout> m_materialDescriptorSetLayout;

    std::shared_ptr<Image2D> m_missingTextureImage;
    std::shared_ptr<Material> m_missingTextureMaterial;

    std::unordered_map<Material*, uint32_t> m_materialIndices;
    std::unordered_map<Texture*, uint32_t> m_textureDescriptorIndices;
    std::vector<GPUMaterial> m_materials;
    std::vector<Texture*> m_materialBufferTextures;
    std::vector<vk::ImageLayout> m_materialBufferImageLayouts;
    size_t m_numRenderEntities;


    std::unordered_map<RenderComponent::UpdateType, bool> m_needsSortEntities;
    std::vector<uint32_t> m_sortedModifiedEntities;
    std::vector<std::pair<size_t, size_t>> m_objectBufferModifiedRegions;

//    std::shared_ptr<GraphicsPipeline> m_graphicsPipeline;
};


#endif //WORLDENGINE_SCENERENDERER_H
