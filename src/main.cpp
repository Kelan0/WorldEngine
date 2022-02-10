
#include "core/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"
#include "core/graphics/UniformBuffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

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
	float x = 0.0F;

    void init() override {

		DescriptorPoolConfiguration descriptorPoolConfig;
		descriptorPoolConfig.device = graphics()->getDevice();
		descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		descriptorPool = DescriptorPool::create(descriptorPoolConfig);


		ubo = UniformBuffer::Builder(descriptorPool)
			.addUniformBlock<UBO0>(0, 0, vk::ShaderStageFlagBits::eVertex)
			.addUniformBlock<UBO1>(1, 0, vk::ShaderStageFlagBits::eVertex)
			.build();

		GraphicsPipelineConfiguration pipelineConfig;
		pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
		pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
		pipelineConfig.vertexInputBindings = Vertex::getBindingDescriptions();
		pipelineConfig.vertexInputAttributes = Vertex::getAttributeDescriptions();
		pipelineConfig.descriptorSetLayous.push_back(ubo->getDescriptorSetLayout(0));
		pipelineConfig.descriptorSetLayous.push_back(ubo->getDescriptorSetLayout(1));
		graphics()->initializeGraphicsPipeline(pipelineConfig);

		MeshConfiguration testMeshConfig;
		testMeshConfig.device = graphics()->getDevice();
		testMeshConfig.vertices = {
			{ glm::vec3(-0.5F, -0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
			{ glm::vec3(+0.5F, -0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
			{ glm::vec3(+0.5F, +0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) },
			{ glm::vec3(-0.5F, +0.5F, 0.0F), glm::vec3(0.0F), glm::vec2(0.0F) }
		};

		testMeshConfig.indices = {
			0, 1, 2, 0, 2, 3
		};

		testMesh = Mesh::create(testMeshConfig);

    }

	void cleanup() override {
		delete testMesh;
		delete ubo;
		descriptorPool.reset();
	}

    void render() override {

		if (graphics()->beginFrame()) {

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
			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0F), glm::vec3(glm::sin(x), 0.0F, 0.0F));
			glm::mat4 viewProjectionMatrix = glm::rotate(glm::mat4(1.0F), glm::radians(45.0F), glm::vec3(0.0F, 0.0F, 1.0F));

			ubo->update(0, 0, &modelMatrix);
			ubo->update(1, 0, &viewProjectionMatrix);

			ubo->bind(0, commandBuffer, pipeline);
			ubo->bind(1, commandBuffer, pipeline);

			testMesh->draw(commandBuffer);

			commandBuffer.endRenderPass();

			graphics()->endFrame();
		}
    }
};

int main(int argc, char* argv[]) {
    return Application::create<App>();
}