#pragma once

#include "../../core.h"
#include "RenderCamera.h"
#include "../../graphics/FrameResource.h"

class Scene;
class Buffer;
class DescriptorSet;
class Texture2D;
class Image2D;

struct CameraInfoUBO {
	glm::mat4 viewProjectionMatrix;
};

struct WorldTransformUBO {
	glm::mat4 modelMatrix;
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

private:
	void recordRenderCommands(double dt, vk::CommandBuffer commandBuffer);

	std::shared_ptr<DescriptorSet> globalDescriptorSet();
	
	std::shared_ptr<DescriptorSet> objectDescriptorSet();

	std::shared_ptr<DescriptorSet> materialDescriptorSet();

	void initMissingTexture();

private:
	Scene* m_scene;
	RenderCamera m_renderCamera;

	FrameResource<Buffer> m_cameraInfoBuffer;
	FrameResource<Buffer> m_worldTransformBuffer;
	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> m_globalDescriptorSet;
	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> m_objectDescriptorSet;
	std::array<std::shared_ptr<DescriptorSet>, CONCURRENT_FRAMES> m_materialDescriptorSet;

	std::shared_ptr<Image2D> m_missingTextureImage;
	std::shared_ptr<Texture2D> m_missingTexture;
};

