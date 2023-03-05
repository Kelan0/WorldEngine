#ifndef WORLDENGINE_SCENERENDERER_H
#define WORLDENGINE_SCENERENDERER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/GraphicsPipeline.h"
#include "core/engine/renderer/RenderCamera.h"
#include "core/engine/scene/Scene.h"
#include "core/engine/renderer/RenderComponent.h"

class Buffer;
class DescriptorSet;
class DescriptorSetLayout;
class Texture;
class Material;
class Image2D;

class SceneRenderer {
public:
    SceneRenderer();

    ~SceneRenderer();

    bool init();

    void preRender(const double& dt);

    void render(const double& dt, const vk::CommandBuffer& commandBuffer, RenderCamera* renderCamera);

    void setScene(Scene* scene);

    Scene* getScene() const;

    const SharedResource<DescriptorSetLayout>& getObjectDescriptorSetLayout() const;

    const SharedResource<DescriptorSetLayout>& getMaterialDescriptorSetLayout() const;

    DescriptorSet* getObjectDescriptorSet() const;

    DescriptorSet* getMaterialDescriptorSet() const;

    uint32_t registerTexture(Texture* texture);

    uint32_t registerMaterial(Material* material);

private:
    void initMissingTextureMaterial();

    void onRenderComponentAdded(ComponentAddedEvent<RenderComponent>* event);

    void onRenderComponentRemoved(ComponentRemovedEvent<RenderComponent>* event);

    void recordRenderCommands(const double& dt, const vk::CommandBuffer& commandBuffer);

    void sortRenderEntities();

    void updateEntityWorldTransforms();

    void updateEntityMaterials();

    void streamEntityRenderData();

    void* mapObjectDataBuffer(const size_t& maxObjects);

    void* mapMaterialDataBuffer(const size_t& maxObjects);
private:
    struct GPUObjectData {
        glm::mat4 prevModelMatrix;
        glm::mat4 modelMatrix;
        uint32_t materialIndex;
        uint32_t _pad0;
        uint32_t _pad1;
        uint32_t _pad2;
    };

    struct GPUMaterial {
        uint32_t albedoTextureIndex;
        uint32_t roughnessTextureIndex;
        uint32_t metallicTextureIndex;
        uint32_t emissionTextureIndex;
        uint32_t normalTextureIndex;
        union {
            uint32_t packedAlbedoColour; // X-R-G-B
            struct {
                uint8_t albedoColour_b;
                uint8_t albedoColour_g;
                uint8_t albedoColour_r;
                uint8_t _unused0;
            };
        };
        union {
            uint32_t packedRoughnessMetallicEmissionR; // EmR-Mtl-Rgh
            struct {
                uint8_t roughness;
                uint8_t metallic;
                uint16_t emission_r;
            };
        };
        union {
            uint32_t packedEmissionGB; // EmGB
            struct {
                uint16_t emission_g;
                uint16_t emission_b;
            };
        };

        union {
            uint32_t packedFlags;
            struct {
                bool hasAlbedoTexture : 1;
                bool hasRoughnessTexture : 1;
                bool hasMetallicTexture : 1;
                bool hasEmissionTexture : 1;
                bool hasNormalTexture : 1;
            };
        };
        uint32_t _pad0;
        uint32_t _pad1;
        uint32_t _pad2;
    };

    struct RenderResources {
        Buffer* worldTransformBuffer;
        Buffer* materialDataBuffer;
        DescriptorSet* objectDescriptorSet;
        DescriptorSet* materialDescriptorSet;
        uint32_t updateTextureDescriptorStartIndex;
        uint32_t updateTextureDescriptorEndIndex;
    };

    struct DrawCommand {
        Mesh* mesh = nullptr;
        uint32_t instanceCount = 0;
        uint32_t firstInstance = 0;
    };

    struct RenderInfo {
        ResourceId materialId = 0;
        uint32_t materialIndex = UINT32_MAX;
        uint32_t objectIndex = UINT32_MAX;
    };

private:
    Scene* m_scene;

    FrameResource<RenderResources> m_resources;

    SharedResource<DescriptorSetLayout> m_objectDescriptorSetLayout;
    SharedResource<DescriptorSetLayout> m_materialDescriptorSetLayout;

    std::shared_ptr<Image2D> m_missingTextureImage;
    std::shared_ptr<Material> m_missingTextureMaterial;

    uint32_t m_numRenderEntities;

    std::unordered_map<ResourceId, uint32_t> m_materialIndices;
    std::unordered_map<ResourceId, uint32_t> m_textureIndices;
    std::vector<ResourceId> m_objectMaterials;
    std::vector<Texture*> m_textures;
    std::vector<GPUObjectData> m_objectBuffer;
    std::vector<GPUMaterial> m_materialBuffer;
    std::vector<DrawCommand> m_drawCommands;

    double m_previousPartialTicks;

    uint32_t m_numAddedRenderEntities;
};


#endif //WORLDENGINE_SCENERENDERER_H
