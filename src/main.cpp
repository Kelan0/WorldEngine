
#include "core/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Mesh.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

class App : public Application {

    Buffer* ubo = NULL;
    std::shared_ptr<DescriptorSet> uboDescriptorSet = NULL;
    std::shared_ptr<DescriptorPool> descriptorPool = NULL;
	Mesh* testMesh = NULL;
	float x = 0.0F;

    void init() override {
		BufferConfiguration uboConfiguration;
		uboConfiguration.device = graphics()->getDevice();
		uboConfiguration.size = sizeof(glm::mat4) * 1;
		uboConfiguration.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		uboConfiguration.usage = vk::BufferUsageFlagBits::eUniformBuffer;
		ubo = Buffer::create(uboConfiguration);

		DescriptorPoolConfiguration descriptorPoolConfig;
		descriptorPoolConfig.device = graphics()->getDevice();
		descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
		descriptorPool = DescriptorPool::create(descriptorPoolConfig);

		std::vector<vk::DescriptorSetLayoutBinding> bindings;
		bindings.emplace_back().setBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);

		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.setBindings(bindings);
		uboDescriptorSet = DescriptorSet::get(layoutCreateInfo, descriptorPool);
		DescriptorSetWriter(uboDescriptorSet).writeBuffer(0, ubo).write();

		GraphicsPipelineConfiguration pipelineConfig;
		pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
		pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
		pipelineConfig.vertexInputBindings = Vertex::getBindingDescriptions();
		pipelineConfig.vertexInputAttributes = Vertex::getAttributeDescriptions();
		pipelineConfig.descriptorSetLayous.push_back(uboDescriptorSet->getLayout()->getDescriptorSetLayout());
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
		uboDescriptorSet.reset();
		descriptorPool.reset();
	}

    void render() override {

		vk::CommandBuffer commandBuffer;
		vk::Framebuffer framebuffer;
		if (graphics()->beginFrame(commandBuffer, framebuffer)) {

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
			//commandBuffer.draw(3, 1, 0, 0);

			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0F), glm::vec3(glm::sin(x), 0.0F, 0.0F));
			x += 1.0F / 8000.0F;
			ubo->upload(0, 1, &modelMatrix);

			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getPipelineLayout(), 0, 1, &uboDescriptorSet->getDescriptorSet(), 0, NULL);

			testMesh->draw(commandBuffer);

			commandBuffer.endRenderPass();

			graphics()->endFrame();
		}
    }
};

int main(int argc, char* argv[]) {
    return Application::create<App>();
}