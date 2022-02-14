#pragma once

#include "../core.h"

struct GraphicsPipelineConfiguration {
	std::weak_ptr<vkr::Device> device;
	uint32_t framebufferWidth = 0;
	uint32_t framebufferHeight = 0;
	std::optional<std::string> vertexShader;
	std::optional<std::string> fragmentShader;
	std::vector<vk::VertexInputBindingDescription> vertexInputBindings;
	std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes;
	std::vector<vk::DescriptorSetLayout> descriptorSetLayous;
	vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
	vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
	vk::FrontFace frontFace = vk::FrontFace::eClockwise;
};

class GraphicsPipeline {
	NO_COPY(GraphicsPipeline);

private:
	GraphicsPipeline(std::weak_ptr<vkr::Device> device,
					std::unique_ptr<vkr::Pipeline>& pipeline, 
					std::unique_ptr<vkr::RenderPass>& renderPass, 
					std::unique_ptr<vkr::PipelineLayout>& pipelineLayout);

public:
	~GraphicsPipeline();

	static GraphicsPipeline* create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

	void bind(const vk::CommandBuffer& commandBuffer);

	const vk::Pipeline& getPipeline() const;

	const vk::RenderPass& getRenderPass() const;
	
	const vk::PipelineLayout& getPipelineLayout() const;

private:
	static bool loadShaderStage(std::string filePath, std::vector<char>& bytecode);

	static bool runCommand(const std::string& command);

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unique_ptr<vkr::Pipeline> m_pipeline;
	std::unique_ptr<vkr::RenderPass> m_renderPass;
	std::unique_ptr<vkr::PipelineLayout> m_pipelineLayout;
};

