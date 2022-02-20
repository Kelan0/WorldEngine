#include "ShaderProgram.h"

ShaderProgram::ShaderProgram(std::weak_ptr<ShaderResources> shaderResources, std::weak_ptr<GraphicsPipeline> pipeline):
    m_shaderResources(shaderResources),
    m_pipeline(pipeline) {
}

ShaderProgram::~ShaderProgram() {
}

ShaderProgram* ShaderProgram::create(std::weak_ptr<ShaderResources> shaderResources, std::weak_ptr<GraphicsPipeline> pipeline) {
    if (shaderResources.expired() || shaderResources.lock() == NULL)
        return NULL;
    
    if (pipeline.expired() || pipeline.lock() == NULL)
        return NULL;

    return new ShaderProgram(shaderResources, pipeline);
}

ShaderProgram* ShaderProgram::create(const ShaderResources::Builder& shaderResourcesBuilder, std::weak_ptr<GraphicsPipeline> pipeline) {
    ShaderResources* shaderResources = shaderResourcesBuilder.build();
    if (shaderResources == NULL)
        return NULL;

    ShaderProgram* shaderProgram = create(std::shared_ptr<ShaderResources>(shaderResources), pipeline);
    if (shaderProgram == NULL)
        delete shaderResources;

    return shaderProgram;
}

ShaderProgram* ShaderProgram::create(std::weak_ptr<ShaderResources> shaderResources, const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
    GraphicsPipeline* graphicsPipeline = GraphicsPipeline::create(graphicsPipelineConfiguration);
    if (graphicsPipeline == NULL)
        return NULL;

    ShaderProgram* shaderProgram = create(shaderResources, std::shared_ptr<GraphicsPipeline>(graphicsPipeline));
    if (shaderProgram == NULL)
        delete graphicsPipeline;

    return shaderProgram;
}

ShaderProgram* ShaderProgram::create(const ShaderResources::Builder& shaderResourcesBuilder, const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
    ShaderResources* shaderResources = shaderResourcesBuilder.build();
    if (shaderResources == NULL)
        return NULL;

    GraphicsPipeline* graphicsPipeline = GraphicsPipeline::create(graphicsPipelineConfiguration);
    if (graphicsPipeline == NULL)
        return NULL;

    ShaderProgram* shaderProgram = create(std::shared_ptr<ShaderResources>(shaderResources), std::shared_ptr<GraphicsPipeline>(graphicsPipeline));
    if (shaderProgram == NULL) {
        delete shaderResources;
        delete graphicsPipeline;
    }

    return shaderProgram;
}

std::shared_ptr<ShaderResources> ShaderProgram::getShaderResources() const {
    return m_shaderResources;
}

std::shared_ptr<GraphicsPipeline> ShaderProgram::getPipeline() const {
    return m_pipeline;
}
