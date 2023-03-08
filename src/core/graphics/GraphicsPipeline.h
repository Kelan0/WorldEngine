
#ifndef WORLDENGINE_GRAPHICSPIPELINE_H
#define WORLDENGINE_GRAPHICSPIPELINE_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

#define ALWAYS_RELOAD_SHADERS

class RenderPass;
class Buffer;
class DescriptorSetLayout;
struct ShaderLoadedEvent;

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

    AttachmentBlendState(bool blendEnable, vk::ColorComponentFlags colourWriteMask);
    AttachmentBlendState(bool blendEnable, uint32_t colourWriteMask);
    AttachmentBlendState() = default;

    void setColourWriteMask(vk::ColorComponentFlags colourWriteMask);
    void setColourWriteMask(uint32_t colourWriteMask);

    void setColourBlendMode(const BlendMode& blendMode);
    void setColourBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);

    void setAlphaBlendMode(const BlendMode& blendMode);
    void setAlphaBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);
};

struct DepthBias {
    float constant = 0.0F;
    float slope = 0.0F;
    float clamp = 0.0F;
};

struct GraphicsPipelineConfiguration {
    WeakResource<vkr::Device> device;
    vk::Viewport viewport;
    uint32_t subpass = 0;
    std::optional<std::string> vertexShader;
    std::string vertexShaderEntryPoint;
    std::optional<std::string> fragmentShader;
    std::string fragmentShaderEntryPoint;
    std::vector<vk::VertexInputBindingDescription> vertexInputBindings;
    std::vector<vk::VertexInputAttributeDescription> vertexInputAttributes;
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    std::vector<vk::PushConstantRange> pushConstantRanges;
    vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    DepthBias depthBias;
    bool depthBiasEnable = false;
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    vk::FrontFace frontFace = vk::FrontFace::eClockwise;
    vk::PrimitiveTopology primitiveTopology = vk::PrimitiveTopology::eTriangleList;
    std::vector<AttachmentBlendState> attachmentBlendStates;
    glm::vec4 blendConstants = glm::vec4(0.0F, 0.0F, 0.0F, 0.0F);
    WeakResource<RenderPass> renderPass;
    std::unordered_map<vk::DynamicState, bool> dynamicStates;

    void setViewport(const vk::Viewport& viewport);
    void setViewport(const glm::vec2& size, const glm::vec2& offset = glm::vec2(0.0F, 0.0F), float minDepth = 0.0F, float maxDepth = 1.0F);
    void setViewport(float width, float height, float x = 0.0F, float y = 0.0F, float minDepth = 0.0F, float maxDepth = 1.0F);

    void addVertexInputBinding(const vk::VertexInputBindingDescription& vertexInputBinding);

    void addVertexInputBinding(uint32_t binding, uint32_t stride, vk::VertexInputRate vertexInputRate);

    void setVertexInputBindings(const vk::ArrayProxy<const vk::VertexInputBindingDescription>& vertexInputBindings);

    void addVertexInputAttribute(const vk::VertexInputAttributeDescription& vertexInputAttribute);

    void addVertexInputAttribute(uint32_t location, uint32_t binding, vk::Format format, uint32_t offset);

    void setVertexInputAttribute(const vk::ArrayProxy<const vk::VertexInputAttributeDescription>& vertexInputAttributes);

    void addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout);

    void addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& descriptorSetLayouts);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& descriptorSetLayouts);

    void addPushConstantRange(const vk::PushConstantRange& pushConstantRange);

    void addPushConstantRange(vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

    void setDynamicState(vk::DynamicState dynamicState, bool isDynamic);

    void setDynamicStates(const vk::ArrayProxy<vk::DynamicState>& dynamicState, bool isDynamic);

    AttachmentBlendState& attachmentBlendState(size_t attachmentIndex);

    void setAttachmentBlendState(size_t attachmentIndex, const AttachmentBlendState& attachmentBlendState);

    void setAttachmentBlendEnabled(size_t attachmentIndex, bool blendEnabled);
    void setAttachmentColourWriteMask(size_t attachmentIndex, vk::ColorComponentFlags colourWriteMask);
    void setAttachmentColourWriteMask(size_t attachmentIndex, uint32_t colourWriteMask);

    void setAttachmentColourBlendMode(size_t attachmentIndex, const BlendMode& blendMode);
    void setAttachmentColourBlendMode(size_t attachmentIndex, vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);

    void setAttachmentAlphaBlendMode(size_t attachmentIndex, const BlendMode& blendMode);
    void setAttachmentAlphaBlendMode(size_t attachmentIndex, vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op);
};

class GraphicsPipeline : public GraphicsResource {
    NO_COPY(GraphicsPipeline);

private:
    explicit GraphicsPipeline(const WeakResource<vkr::Device>& device, const std::string& name);

public:
    ~GraphicsPipeline() override;

    static GraphicsPipeline* create(const WeakResource<vkr::Device>& device, const std::string& name);

    static GraphicsPipeline* create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const std::string& name);

    bool recreate(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const std::string& name);

    void bind(const vk::CommandBuffer& commandBuffer) const;

    const vk::Pipeline& getPipeline() const;

    const SharedResource<RenderPass>& getRenderPass() const;

    const vk::PipelineLayout& getPipelineLayout() const;

    const GraphicsPipelineConfiguration& getConfig() const;

    bool isValid() const;

    bool isStateDynamic(vk::DynamicState dynamicState) const;

    void setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const vk::Viewport* viewports);
    void setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const std::vector<vk::Viewport>& viewports);
    void setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const vk::Viewport& viewport);
    void setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, float width, float height, float x = 0.0F, float y = 0.0F, float minDepth = 0.0F, float maxDepth = 1.0F);
    void setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const glm::vec2& size, const glm::vec2& offset = glm::vec2(0.0F), float minDepth = 0.0F, float maxDepth = 1.0F);

    void setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const vk::Rect2D* scissorRects);
    void setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const std::vector<vk::Rect2D>& scissorRects);
    void setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const vk::Rect2D& scissorRect);
    void setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, int32_t x, int32_t y, uint32_t width, uint32_t height);
    void setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const glm::ivec2& offset, const glm::uvec2& size);

    void setLineWidth(const vk::CommandBuffer& commandBuffer, float lineWidth);

    void setDepthBias(const vk::CommandBuffer& commandBuffer, float constantFactor, float clamp, float slopeFactor);

    void setBlendConstants(const vk::CommandBuffer& commandBuffer, const glm::vec4& constants);
    void setBlendConstants(const vk::CommandBuffer& commandBuffer, float r, float g, float b, float a);

    void setDepthBounds(const vk::CommandBuffer& commandBuffer, float minDepthBound, float maxDepthBound);

    void setStencilCompareMask(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t compareMask);

    void setStencilWriteMask(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t writeMask);

    void setStencilReference(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t reference);

    void setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleLocationsInfoEXT& sampleLocations);
    void setSampleLocations(const vk::CommandBuffer& commandBuffer, vk::SampleCountFlagBits samplesPerPixel, const vk::Extent2D& sampleGridSize, uint32_t sampleLocationsCount, const vk::SampleLocationEXT* sampleLocations);
    void setSampleLocations(const vk::CommandBuffer& commandBuffer, vk::SampleCountFlagBits samplesPerPixel, const vk::Extent2D& sampleGridSize, const std::vector<vk::SampleLocationEXT>& sampleLocations);

    void setLineStipple(const vk::CommandBuffer& commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern);

    void setCullMode(const vk::CommandBuffer& commandBuffer, vk::CullModeFlags cullMode);

    void setFrontFace(const vk::CommandBuffer& commandBuffer, vk::FrontFace frontFace);

    void setPrimitiveTopology(const vk::CommandBuffer& commandBuffer, vk::PrimitiveTopology primitiveTopology);

    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides);
    void bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides);

    void setDepthTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setDepthWriteEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setDepthCompareOp(const vk::CommandBuffer& commandBuffer, vk::CompareOp compareOp);

    void setDepthBoundsTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setStencilTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setStencilOp(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, vk::StencilOp failOp, vk::StencilOp passOp, vk::StencilOp depthFailOp, vk::CompareOp compareOp);

    void setVertexInput(const vk::CommandBuffer& commandBuffer, uint32_t vertexBindingCount, const vk::VertexInputBindingDescription2EXT* vertexBindings, uint32_t vertexAttribCount, const vk::VertexInputAttributeDescription2EXT* vertexAttribs);

    void setRasterizerDiscardEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setDepthBiasEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setLogicOp(const vk::CommandBuffer& commandBuffer, vk::LogicOp logicOp);

    void setPrimitiveRestartEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    void setColourWriteEnabled(const vk::CommandBuffer& commandBuffer, bool enabled);

    static vk::Viewport getScreenViewport(const vk::Viewport& viewport);
    static vk::Viewport getScreenViewport(float width, float height, float x = 0.0F, float y = 0.0F, float minDepth = 0.0F, float maxDepth = 1.0F);
    static vk::Viewport getScreenViewport(const glm::vec2& size, const glm::vec2& offset = glm::vec2(0.0F), float minDepth = 0.0F, float maxDepth = 1.0F);

private:
    void cleanup();

    void validateDynamicState(vk::DynamicState dynamicState);

#if ENABLE_SHADER_HOT_RELOAD
    void onShaderLoaded(ShaderLoadedEvent* event);
#endif

private:
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipelineLayout;
    SharedResource<RenderPass> m_renderPass;
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
