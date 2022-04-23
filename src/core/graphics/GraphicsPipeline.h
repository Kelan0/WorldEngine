
#ifndef WORLDENGINE_GRAPHICSPIPELINE_H
#define WORLDENGINE_GRAPHICSPIPELINE_H

#include "core/core.h"
#include "core/graphics/RenderPass.h"

struct BlendMode {
    vk::BlendFactor src = vk::BlendFactor::eOne;
    vk::BlendFactor dst = vk::BlendFactor::eZero;
    vk::BlendOp op = vk::BlendOp::eAdd;
};

struct AttachmentBlendState {
    bool blendEnable = false;
    BlendMode colourBlendMode;
    BlendMode alphaBlendMode;
    vk::ColorComponentFlags colourWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    void setColourBlendMode(const BlendMode& blendMode);
    void setColourBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);

    void setAlphaBlendMode(const BlendMode& blendMode);
    void setAlphaBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);
};

struct GraphicsPipelineConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::Viewport viewport;
    std::optional<std::string> vertexShader;
    std::optional<std::string> fragmentShader;
    std::vector<vk::VertexInputBindingDescription> vertexInputBindings;
    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes;
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    vk::FrontFace frontFace = vk::FrontFace::eClockwise;
    vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
    std::vector<AttachmentBlendState> attachmentBlendStates;
    std::weak_ptr<RenderPass> renderPass;
    std::unordered_map<vk::DynamicState, bool> dynamicStates;

    void setViewport(float width, float height, float x = 0.0F, float y = 0.0F, float minDepth = 0.0F, float maxDepth = 1.0F);

    void setDynamicState(const vk::DynamicState& dynamicState, const bool& isDynamic);

    void setDynamicStates(const std::vector<vk::DynamicState>& dynamicState, const bool& isDynamic);

    void setAttachmentBlendState(const size_t& attachmentIndex, const AttachmentBlendState& attachmentBlendState);
};

class GraphicsPipeline {
    NO_COPY(GraphicsPipeline);

private:
    GraphicsPipeline(std::weak_ptr<vkr::Device> device);

    GraphicsPipeline(std::weak_ptr<vkr::Device> device,
                     std::unique_ptr<vkr::Pipeline>& pipeline,
                     std::shared_ptr<RenderPass>& renderPass,
                     std::unique_ptr<vkr::PipelineLayout>& pipelineLayout,
                     const GraphicsPipelineConfiguration& config);

public:
    ~GraphicsPipeline();

    static GraphicsPipeline* create(std::weak_ptr<vkr::Device> device);

    static GraphicsPipeline* create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

    bool recreate(GraphicsPipelineConfiguration graphicsPipelineConfiguration);

    void bind(const vk::CommandBuffer& commandBuffer);

    const vk::Pipeline& getPipeline() const;

    std::shared_ptr<RenderPass> getRenderPass() const;

    const vk::PipelineLayout& getPipelineLayout() const;

    const GraphicsPipelineConfiguration& getConfig() const;

    bool isValid() const;

    bool isStateDynamic(const vk::DynamicState& dynamicState) const;

    void setDepthTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setCullMode(const vk::CommandBuffer& commandBuffer, const vk::CullModeFlags& cullMode);
private:
    static bool loadShaderStage(std::string filePath, std::vector<char>& bytecode);

    static bool runCommand(const std::string& command);

private:
    std::shared_ptr<vkr::Device> m_device;
    std::unique_ptr<vkr::Pipeline> m_pipeline;
    std::shared_ptr<RenderPass> m_renderPass;
    std::unique_ptr<vkr::PipelineLayout> m_pipelineLayout;
    GraphicsPipelineConfiguration m_config;
};




namespace std {
    template<>
    struct hash<BlendMode> {
        size_t operator()(const BlendMode& blendMode) const {
            size_t hash = 0;
            std::hash_combine(hash, (uint32_t)blendMode.src);
            std::hash_combine(hash, (uint32_t)blendMode.dst);
            std::hash_combine(hash, (uint32_t)blendMode.op);
            return hash;
        }
    };
    template<>
    struct hash<AttachmentBlendState> {
        size_t operator()(const AttachmentBlendState& attachmentBlendState) const {
            size_t hash = 0;
            std::hash_combine(hash, attachmentBlendState.blendEnable);
            std::hash_combine(hash, attachmentBlendState.colourBlendMode);
            std::hash_combine(hash, attachmentBlendState.alphaBlendMode);
            std::hash_combine(hash, (uint32_t)attachmentBlendState.colourWriteMask);
            return hash;
        }
    };
//    template<>
//    struct hash<GraphicsPipelineConfiguration> {
//        size_t operator()(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) const {
//            size_t hash = 0;
//            // TODO
//            return hash;
//        }
//    };
}


#endif //WORLDENGINE_GRAPHICSPIPELINE_H
