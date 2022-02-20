#pragma once

#include "../core.h"
#include "ShaderResources.h"
#include "GraphicsPipeline.h"

class ShaderProgram {
private:
	ShaderProgram(std::weak_ptr<ShaderResources> shaderResources, std::weak_ptr<GraphicsPipeline> pipeline);

public:
	~ShaderProgram();

	static ShaderProgram* create(std::weak_ptr<ShaderResources> shaderResources, std::weak_ptr<GraphicsPipeline> pipeline);

	static ShaderProgram* create(const ShaderResources::Builder& shaderResourcesBuilder, std::weak_ptr<GraphicsPipeline> pipeline);

	static ShaderProgram* create(std::weak_ptr<ShaderResources> shaderResources, const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

	static ShaderProgram* create(const ShaderResources::Builder& shaderResourcesBuilder, const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

	std::shared_ptr<ShaderResources> getShaderResources() const;

	std::shared_ptr<GraphicsPipeline> getPipeline() const;

private:
	std::shared_ptr<ShaderResources> m_shaderResources;
	std::shared_ptr<GraphicsPipeline> m_pipeline;
};

