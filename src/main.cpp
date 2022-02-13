
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/UniformBuffer.h"
#include "core/graphics/Texture.h"
#include "core/engine/Camera.h"

#include <iostream>

struct UBO0 {
	glm::mat4 modelMatrix = glm::mat4(1.0F);
};
struct UBO1 {
	glm::mat4 viewProjectionMatrix = glm::mat4(1.0F);
};

class App : public Application {

	UniformBuffer* ubo;
	std::shared_ptr<DescriptorPool> descriptorPool;
	Mesh* testMesh = NULL;
	Image2D* testImage = NULL;
	Texture2D* testTexture = NULL;
	Camera camera;
	

	float x = 0.0F;

    void init() override {

		Image2DConfiguration testTextureImageConfig;
		testTextureImageConfig.device = graphics()->getDevice();
		testTextureImageConfig.filePath = "D:/Code/ActiveProjects/WorldEngine/res/textures/Brick_Wall_017_SD/Brick_Wall_017_basecolor.jpg";
		testTextureImageConfig.usage = vk::ImageUsageFlagBits::eSampled;
		testTextureImageConfig.format = vk::Format::eR8G8B8A8Srgb;
		testImage = Image2D::create(testTextureImageConfig);

		SamplerConfiguration testTextureSamplerConfig;
		testTextureSamplerConfig.device = graphics()->getDevice();
		testTextureSamplerConfig.minFilter = vk::Filter::eLinear;
		testTextureSamplerConfig.magFilter = vk::Filter::eLinear;
		ImageView2DConfiguration testTextureImageViewConfig;
		testTextureImageViewConfig.device = graphics()->getDevice();
		testTextureImageViewConfig.image = testImage->getImage();
		testTextureImageViewConfig.format = testImage->getFormat();
		testTexture = Texture2D::create(testTextureImageViewConfig, testTextureSamplerConfig);

		MeshConfiguration testMeshConfig;
		testMeshConfig.device = graphics()->getDevice();
		testMeshConfig.vertices = {
			{ glm::vec3(-1.0F, -1.0F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F, 0.0F) },
			{ glm::vec3(+1.0F, -1.0F, 0.0F), glm::vec3(0.0F), glm::vec2(1.0F, 0.0F) },
			{ glm::vec3(+1.0F, +1.0F, 0.0F), glm::vec3(0.0F), glm::vec2(1.0F, 1.0F) },
			{ glm::vec3(-1.0F, +1.0F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F, 1.0F) }
		};
		testMeshConfig.indices = { 0, 1, 2, 0, 2, 3 };
		testMesh = Mesh::create(testMeshConfig);


		DescriptorPoolConfiguration descriptorPoolConfig;
		descriptorPoolConfig.device = graphics()->getDevice();
		descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		descriptorPool = std::shared_ptr<DescriptorPool>(DescriptorPool::create(descriptorPoolConfig));


		ubo = UniformBuffer::Builder(descriptorPool)
			.addUniformBlock<UBO0>(0, 0, vk::ShaderStageFlagBits::eVertex)
			.addUniformBlock<UBO1>(1, 0, vk::ShaderStageFlagBits::eVertex)
			.addTextureSampler(2, 0, vk::ShaderStageFlagBits::eFragment)
			.build();

		ubo->startBatchWrite(2);
		ubo->writeImage(2, 0, testTexture, vk::ImageLayout::eShaderReadOnlyOptimal);
		ubo->endBatchWrite(2);

		GraphicsPipelineConfiguration pipelineConfig;
		pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
		pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
		pipelineConfig.vertexInputBindings = Vertex::getBindingDescriptions();
		pipelineConfig.vertexInputAttributes = Vertex::getAttributeDescriptions();
		ubo->initPipelineConfiguration(pipelineConfig);
		graphics()->initializeGraphicsPipeline(pipelineConfig);
    }

	void cleanup() override {
		delete testTexture;
		delete testImage;
		delete testMesh;
		delete ubo;
		descriptorPool.reset();
	}

    void render() override {

		if (graphics()->beginFrame()) {

			camera.setAspect(graphics()->getAspectRatio());
			camera.setClipppingPlanes(0.001F, 10.0F);
			camera.setFovDegrees(90.0F);
			camera.lookAt(0.0F, 0.0F, 0.5F * glm::sin(x) + 0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F);
			camera.update();

			auto& commandBuffer = graphics()->getCurrentCommandBuffer();
			auto& framebuffer = graphics()->getCurrentFramebuffer();

			GraphicsPipeline& pipeline = graphics()->pipeline();

			vk::ClearValue clearValue;
			clearValue.color.setFloat32({ 0.0F, 0.0F, 0.0F, 1.0F });

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.setRenderPass(pipeline.getRenderPass());
			renderPassBeginInfo.setFramebuffer(framebuffer);
			renderPassBeginInfo.renderArea.setOffset({ 0, 0 });
			renderPassBeginInfo.renderArea.setExtent(graphics()->getImageExtent());
			renderPassBeginInfo.setClearValueCount(1);
			renderPassBeginInfo.setPClearValues(&clearValue);

			commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
			pipeline.bind(commandBuffer);

			x += 1.0F / 8000.0F;
			glm::mat4 modelMatrix(1.0F);// = glm::translate(glm::mat4(1.0F), glm::vec3(glm::sin(x), 0.0F, 0.0F));

			ubo->update(0, 0, &modelMatrix);
			ubo->update(1, 0, &camera.getViewProjectionMatrix());

			ubo->bind({ 0, 1, 2 }, 0, commandBuffer, pipeline);

			testMesh->draw(commandBuffer);

			commandBuffer.endRenderPass();

			graphics()->endFrame();
		}
    }
};

int main(int argc, char* argv[]) {
    return Application::create<App>();
}