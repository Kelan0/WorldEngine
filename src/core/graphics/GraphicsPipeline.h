
#ifndef WORLDENGINE_GRAPHICSPIPELINE_H
#define WORLDENGINE_GRAPHICSPIPELINE_H

#include "core/core.h"

#define ALWAYS_RELOAD_SHADERS

class RenderPass;
class Buffer;
class DescriptorSetLayout;

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

    AttachmentBlendState(const bool& blendEnable, const vk::ColorComponentFlags& colourWriteMask);
    AttachmentBlendState(const bool& blendEnable, const uint32_t& colourWriteMask);
    AttachmentBlendState() = default;

    void setColourBlendMode(const BlendMode& blendMode);
    void setColourBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);

    void setAlphaBlendMode(const BlendMode& blendMode);
    void setAlphaBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op);
};

struct DepthBias {
    float constant = 0.0F;
    float slope = 0.0F;
    float clamp = 0.0F;
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
    DepthBias depthBias;
    bool depthBiasEnable = false;
    vk::FrontFace frontFace = vk::FrontFace::eClockwise;
    vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
    std::vector<AttachmentBlendState> attachmentBlendStates;
    std::weak_ptr<RenderPass> renderPass;
    std::unordered_map<vk::DynamicState, bool> dynamicStates;

    void setViewport(const vk::Viewport& viewport);
    void setViewport(const glm::vec2& size, const glm::vec2& offset = glm::vec2(0.0F, 0.0F), const float& minDepth = 0.0F, const float& maxDepth = 1.0F);
    void setViewport(const float& width, const float& height, const float& x = 0.0F, const float& y = 0.0F, const float& minDepth = 0.0F, const float& maxDepth = 1.0F);

    void addVertexInputBinding(const vk::VertexInputBindingDescription& vertexInputBinding);

    void addVertexInputBinding(const uint32_t& binding, const uint32_t& stride, const vk::VertexInputRate& vertexInputRate);

    void setVertexInputBindings(const vk::ArrayProxy<const vk::VertexInputBindingDescription>& vertexInputBindings);

    void addVertexInputAttribute(const vk::VertexInputAttributeDescription& vertexInputAttribute);

    void addVertexInputAttribute(const uint32_t& location, const uint32_t& binding, const vk::Format& format, const uint32_t& offset);

    void setVertexInputAttribute(const vk::ArrayProxy<const vk::VertexInputAttributeDescription>& vertexInputAttributes);

    void addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout);

    void addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& descriptorSetLayouts);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& descriptorSetLayouts);

    void setDynamicState(const vk::DynamicState& dynamicState, const bool& isDynamic);

    void setDynamicStates(const vk::ArrayProxy<const vk::DynamicState>& dynamicState, const bool& isDynamic);

    void setAttachmentBlendState(const size_t& attachmentIndex, const AttachmentBlendState& attachmentBlendState);
};

class GraphicsPipeline {
    NO_COPY(GraphicsPipeline);

private:
    GraphicsPipeline(std::weak_ptr<vkr::Device> device);

    GraphicsPipeline(std::weak_ptr<vkr::Device> device,
                     vk::Pipeline& pipeline,
                     vk::PipelineLayout& pipelineLayout,
                     std::shared_ptr<RenderPass>& renderPass,
                     const GraphicsPipelineConfiguration& config);

public:
    ~GraphicsPipeline();

    static GraphicsPipeline* create(std::weak_ptr<vkr::Device> device);

    static GraphicsPipeline* create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const char* name);

    bool recreate(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const char* name);

    void bind(const vk::CommandBuffer& commandBuffer) const;

    const vk::Pipeline& getPipeline() const;

    std::shared_ptr<RenderPass> getRenderPass() const;

    const vk::PipelineLayout& getPipelineLayout() const;

    const GraphicsPipelineConfiguration& getConfig() const;

    bool isValid() const;

    bool isStateDynamic(const vk::DynamicState& dynamicState) const;

    void setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const uint32_t& viewportCount, const vk::Viewport* viewports);
    void setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const std::vector<vk::Viewport>& viewports);
    void setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const vk::Viewport& viewport);
    void setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const float& x, const float& y, const float& width, const float& height, const float& minDepth, const float& maxDepth);

    void setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const uint32_t& scissorCount, const vk::Rect2D* scissorRects);
    void setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const std::vector<vk::Rect2D>& scissorRects);
    void setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const vk::Rect2D& scissorRect);
    void setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height);

    void setLineWidth(const vk::CommandBuffer& commandBuffer, const float& lineWidth);

    void setDepthBias(const vk::CommandBuffer& commandBuffer, const float& constantFactor, const float& clamp, const float& slopeFactor);

    void setBlendConstants(const vk::CommandBuffer& commandBuffer, const glm::vec4& constants);
    void setBlendConstants(const vk::CommandBuffer& commandBuffer, const float& r, const float& g, const float& b, const float& a);

    void setDepthBounds(const vk::CommandBuffer& commandBuffer, const float& minDepthBound, const float& maxDepthBound);

    void setStencilCompareMask(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& compareMask);

    void setStencilWriteMask(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& writeMask);

    void setStencilReference(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& reference);

    void setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleLocationsInfoEXT& sampleLocations);
    void setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleCountFlagBits& samplesPerPixel, const vk::Extent2D& sampleGridSize, const uint32_t& sampleLocationsCount, const vk::SampleLocationEXT* sampleLocations);
    void setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleCountFlagBits& samplesPerPixel, const vk::Extent2D& sampleGridSize, const std::vector<vk::SampleLocationEXT>& sampleLocations);

    void setLineStipple(const vk::CommandBuffer& commandBuffer, const uint32_t& lineStippleFactor, const uint16_t& lineStipplePattern);

    void setCullMode(const vk::CommandBuffer& commandBuffer, const vk::CullModeFlags& cullMode);

    void setFrontFace(const vk::CommandBuffer& commandBuffer, const vk::FrontFace& frontFace);

    void setPrimitiveTopology(const vk::CommandBuffer& commandBuffer, const vk::PrimitiveTopology& primitiveTopology);

    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides);

    void setDepthTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setDepthWriteEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setDepthCompareOp(const vk::CommandBuffer& commandBuffer, const vk::CompareOp& compareOp);

    void setDepthBoundsTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setStencilTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setStencilOp(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const vk::StencilOp& failOp, const vk::StencilOp& passOp, const vk::StencilOp& depthFailOp, const vk::CompareOp& compareOp);

    void setVertexInput(const vk::CommandBuffer& commandBuffer, const uint32_t& vertexBindingCount, const vk::VertexInputBindingDescription2EXT* vertexBindings, const uint32_t& vertexAttribCount, const vk::VertexInputAttributeDescription2EXT* vertexAttribs);

    void setRasterizerDiscardEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setDepthBiasEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setLogicOp(const vk::CommandBuffer& commandBuffer, const vk::LogicOp& logicOp);

    void setPrimitiveRestartEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

    void setColourWriteEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled);

private:
    void cleanup();

    void validateDynamicState(const vk::DynamicState& dynamicState);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipelineLayout;
    std::shared_ptr<RenderPass> m_renderPass;
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
