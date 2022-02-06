#pragma once

#include "../core.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include "GraphicsManager.h"

struct GraphicsPipelineConfiguration {
	std::shared_ptr<vkr::Device> device;
	uint32_t width;
	uint32_t height;
	std::optional<std::string> vertexShader;
	std::optional<std::string> fragmentShader;
};

class GraphicsPipeline {
	NO_COPY(GraphicsPipeline);

private:
	GraphicsPipeline(std::shared_ptr<vkr::Device> device, 
					std::unique_ptr<vkr::Pipeline>& pipeline, 
					std::unique_ptr<vkr::RenderPass>& renderPass, 
					std::unique_ptr<vkr::PipelineLayout>& pipelineLayout);

public:
	~GraphicsPipeline();

	static GraphicsPipeline* create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

	void bind(const vk::CommandBuffer& commandBuffer);

	const vkr::Pipeline& get() const;

	const vkr::RenderPass& getRenderPass() const;
	
	const vkr::PipelineLayout& getPipelineLayout();

private:
	static bool loadShaderStage(std::string filePath, std::vector<char>& bytecode);

	static bool runCommand(const std::string& command);

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unique_ptr<vkr::Pipeline> m_pipeline;
	std::unique_ptr<vkr::RenderPass> m_renderPass;
	std::unique_ptr<vkr::PipelineLayout> m_pipelineLayout;
};

