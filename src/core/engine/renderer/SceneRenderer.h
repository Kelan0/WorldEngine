#pragma once

#include "../../core.h"
#include "RenderCamera.h"
#include "../../graphics/FrameResource.h"

#include "../scene/Scene.h"

struct RenderComponent;
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

struct EntityRenderState {
	glm::mat4x3 modelMatrix;
	uint32_t textureIndex;
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

private:
	void recordRenderCommands(double dt, vk::CommandBuffer commandBuffer);
	
	void initMissingTexture();

	void sortRenderableEntities() const;

	void updateEntityWorldTransforms();

	void updateEntityWorldTransformsRange(size_t start, size_t end);

	void updateMaterialsBuffer();

	void onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event);

	void onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event);

	ObjectDataUBO* mappedWorldTransformsBuffer(size_t maxObjects);

	//EntityRenderState& getEntityRenderState(entt::entity id);
private:
	Scene* m_scene;
	RenderCamera m_renderCamera;


	struct RenderResources {
		Buffer* cameraInfoBuffer;
		Buffer* worldTransformBuffer;
		DescriptorSet* globalDescriptorSet;
		DescriptorSet* objectDescriptorSet;
		DescriptorSet* materialDescriptorSet;
		std::vector<entt::entity> objectEntities;
		std::vector<ObjectDataUBO> objectBuffer;
		std::vector<Texture2D*> objectTextures;
		std::vector<Texture2D*> materialBufferTextures;
		std::vector<vk::ImageLayout> materialBufferImageLayouts;
	};

	FrameResource<RenderResources> m_resources;

	std::shared_ptr<DescriptorSetLayout> m_globalDescriptorSetLayout;
	std::shared_ptr<DescriptorSetLayout> m_objectDescriptorSetLayout;
	std::shared_ptr<DescriptorSetLayout> m_materialDescriptorSetLayout;

	std::shared_ptr<Image2D> m_missingTextureImage;
	std::shared_ptr<Texture2D> m_missingTexture;
	bool m_needsSortRenderableEntities;

	std::unordered_map<Texture2D*, uint32_t> m_textureDescriptorIndices;
};

