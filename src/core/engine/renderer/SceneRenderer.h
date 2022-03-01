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

struct ObjectDataOBO {
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

private:
	void recordRenderCommands(double dt, vk::CommandBuffer commandBuffer);
	
	void initMissingTexture();

	void sortRenderableEntities() const;

	void updateEntityWorldTransforms() const;

	void updateMaterialsBuffer();

	void onRenderComponentAdded(const ComponentAddedEvent<RenderComponent>& event);

	void onRenderComponentRemoved(const ComponentRemovedEvent<RenderComponent>& event);

private:
	Scene* m_scene;
	RenderCamera m_renderCamera;

	FrameResource<Buffer> m_cameraInfoBuffer;
	FrameResource<Buffer> m_worldTransformBuffer;
	FrameResource<DescriptorSet> m_globalDescriptorSet;
	FrameResource<DescriptorSet> m_objectDescriptorSet;
	FrameResource<DescriptorSet> m_materialDescriptorSet;

	std::shared_ptr<Image2D> m_missingTextureImage;
	std::shared_ptr<Texture2D> m_missingTexture;
	bool m_needsSortRenderableEntities;

	std::unordered_map<Texture2D*, uint32_t> m_textureDescriptorIndices;
};

