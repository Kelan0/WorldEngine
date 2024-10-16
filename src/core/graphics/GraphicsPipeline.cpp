#include "core/graphics/GraphicsPipeline.h"
#include "core/application/Application.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/ShaderUtils.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/util/Util.h"
#include "core/util/Logger.h"


AttachmentBlendState::AttachmentBlendState(bool blendEnable, vk::ColorComponentFlags colourWriteMask):
        blendEnable(blendEnable) {
    setColourWriteMask(colourWriteMask);
}

AttachmentBlendState::AttachmentBlendState(bool blendEnable, uint32_t colourWriteMask):
        AttachmentBlendState(blendEnable, (vk::ColorComponentFlags)colourWriteMask) {
}

void AttachmentBlendState::setColourWriteMask(vk::ColorComponentFlags p_colourWriteMask) {
//    constexpr uint32_t ValidColourBits = (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    auto ValidColourBits = (vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    assert(!(p_colourWriteMask & (~ValidColourBits))); // Colour write mask must only contain R,G,B or A bits.
    colourWriteMask = p_colourWriteMask;
}

void AttachmentBlendState::setColourWriteMask(uint32_t p_colourWriteMask) {
    setColourWriteMask((vk::ColorComponentFlags)colourWriteMask);
}

void AttachmentBlendState::setColourBlendMode(const BlendMode& blendMode) {
    colourBlendMode = blendMode;
}

void AttachmentBlendState::setColourBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    colourBlendMode.src = src;
    colourBlendMode.dst = dst;
    colourBlendMode.op = op;
}


void AttachmentBlendState::setAlphaBlendMode(const BlendMode& blendMode) {
    alphaBlendMode = blendMode;
}

void AttachmentBlendState::setAlphaBlendMode(vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    alphaBlendMode.src = src;
    alphaBlendMode.dst = dst;
    alphaBlendMode.op = op;
}


void GraphicsPipelineConfiguration::setViewport(const vk::Viewport& p_viewport) {
    viewport = p_viewport;
}

void GraphicsPipelineConfiguration::setViewport(const glm::vec2& size, const glm::vec2& offset, float minDepth, float maxDepth) {
    setViewport(vk::Viewport(offset.x, offset.y, size.x, size.y, minDepth, maxDepth));
}

void GraphicsPipelineConfiguration::setViewport(float width, float height, float x, float y, float minDepth, float maxDepth) {
    setViewport(vk::Viewport(x, y, width, height, minDepth, maxDepth));
}

void GraphicsPipelineConfiguration::addVertexInputBinding(const vk::VertexInputBindingDescription& vertexInputBinding) {
    vertexInputBindings.emplace_back(vertexInputBinding);
}

void GraphicsPipelineConfiguration::addVertexInputBinding(uint32_t binding, uint32_t stride, vk::VertexInputRate vertexInputRate) {
    addVertexInputBinding(vk::VertexInputBindingDescription(binding, stride, vertexInputRate));
}

void GraphicsPipelineConfiguration::setVertexInputBindings(const vk::ArrayProxy<const vk::VertexInputBindingDescription>& p_vertexInputBindings) {
    vertexInputBindings.clear();
    for (const auto& vertexInputBinding : p_vertexInputBindings)
        addVertexInputBinding(vertexInputBinding);
}

void GraphicsPipelineConfiguration::addVertexInputAttribute(const vk::VertexInputAttributeDescription& vertexInputAttribute) {
    vertexInputAttributes.emplace_back(vertexInputAttribute);
}

void GraphicsPipelineConfiguration::addVertexInputAttribute(uint32_t location, uint32_t binding, vk::Format format, uint32_t offset) {
    addVertexInputAttribute(vk::VertexInputAttributeDescription(location, binding, format, offset));
}

void GraphicsPipelineConfiguration::setVertexInputAttribute(const vk::ArrayProxy<const vk::VertexInputAttributeDescription>& p_vertexInputAttributes) {
    vertexInputAttributes.clear();
    for (const auto& vertexInputAttribute : p_vertexInputAttributes)
        addVertexInputAttribute(vertexInputAttribute);
}

void GraphicsPipelineConfiguration::addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout) {
    assert(descriptorSetLayout);
    descriptorSetLayouts.emplace_back(descriptorSetLayout);
}

void GraphicsPipelineConfiguration::addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout) {
    assert(descriptorSetLayout != nullptr);
    addDescriptorSetLayout(descriptorSetLayout->getDescriptorSetLayout());
}

void GraphicsPipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& p_descriptorSetLayouts) {
    descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : p_descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void GraphicsPipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& p_descriptorSetLayouts) {
    descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : p_descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void GraphicsPipelineConfiguration::addPushConstantRange(const vk::PushConstantRange& pushConstantRange) {
    pushConstantRanges.emplace_back(pushConstantRange);
}

void GraphicsPipelineConfiguration::addPushConstantRange(vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    addPushConstantRange(vk::PushConstantRange(stageFlags, offset, size));
}

void GraphicsPipelineConfiguration::setDynamicState(vk::DynamicState dynamicState, bool isDynamic) {
    dynamicStates[dynamicState] = isDynamic;
}

void GraphicsPipelineConfiguration::setDynamicStates(const vk::ArrayProxy<vk::DynamicState>& dynamicStates, bool isDynamic) {
    for (const auto& state : dynamicStates)
        setDynamicState(state, isDynamic);
}

AttachmentBlendState& GraphicsPipelineConfiguration::attachmentBlendState(size_t attachmentIndex) {
//    assert(attachmentIndex < renderPass.lock()->getAttachmentCount());
    while (attachmentBlendStates.size() <= attachmentIndex)
        attachmentBlendStates.emplace_back();
    return attachmentBlendStates[attachmentIndex];
}

void GraphicsPipelineConfiguration::setAttachmentBlendState(size_t attachmentIndex, const AttachmentBlendState& p_attachmentBlendState) {
    attachmentBlendState(attachmentIndex) = p_attachmentBlendState;
}

void GraphicsPipelineConfiguration::setAttachmentBlendEnabled(size_t attachmentIndex, bool blendEnabled) {
    attachmentBlendState(attachmentIndex).blendEnable = blendEnabled;
}

void GraphicsPipelineConfiguration::setAttachmentColourWriteMask(size_t attachmentIndex, vk::ColorComponentFlags colourWriteMask) {
    attachmentBlendState(attachmentIndex).setColourWriteMask(colourWriteMask);
}

void GraphicsPipelineConfiguration::setAttachmentColourWriteMask(size_t attachmentIndex, uint32_t colourWriteMask) {
    attachmentBlendState(attachmentIndex).setColourWriteMask(colourWriteMask);
}

void GraphicsPipelineConfiguration::setAttachmentColourBlendMode(size_t attachmentIndex, const BlendMode& blendMode) {
    attachmentBlendState(attachmentIndex).setColourBlendMode(blendMode);
}

void GraphicsPipelineConfiguration::setAttachmentColourBlendMode(size_t attachmentIndex, vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    attachmentBlendState(attachmentIndex).setColourBlendMode(src, dst, op);
}

void GraphicsPipelineConfiguration::setAttachmentAlphaBlendMode(size_t attachmentIndex, const BlendMode& blendMode) {
    attachmentBlendState(attachmentIndex).setAlphaBlendMode(blendMode);
}

void GraphicsPipelineConfiguration::setAttachmentAlphaBlendMode(size_t attachmentIndex, vk::BlendFactor src, vk::BlendFactor dst, vk::BlendOp op) {
    attachmentBlendState(attachmentIndex).setAlphaBlendMode(src, dst, op);
}

GraphicsPipeline::GraphicsPipeline(const WeakResource<vkr::Device>& device, const std::string& name):
        GraphicsResource(ResourceType_GraphicsPipeline, device, name),
        m_config(GraphicsPipelineConfiguration{}) {

#if ENABLE_SHADER_HOT_RELOAD
    Engine::eventDispatcher()->connect(&GraphicsPipeline::onShaderLoaded, this);
#endif
}

GraphicsPipeline::~GraphicsPipeline() {
#if ENABLE_SHADER_HOT_RELOAD
    Engine::eventDispatcher()->disconnect(&GraphicsPipeline::onShaderLoaded, this);
#endif

    cleanup();
}

GraphicsPipeline* GraphicsPipeline::create(const WeakResource<vkr::Device>& device, const std::string& name) {
    return new GraphicsPipeline(device, name);
}

GraphicsPipeline* GraphicsPipeline::create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const std::string& name) {
    GraphicsPipeline* graphicsPipeline = create(graphicsPipelineConfiguration.device, name);

    if (!graphicsPipeline->recreate(graphicsPipelineConfiguration, name)) {
        delete graphicsPipeline;
        return nullptr;
    }

    return graphicsPipeline;
}

bool GraphicsPipeline::recreate(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const std::string& name) {
    assert(!graphicsPipelineConfiguration.device.expired() && graphicsPipelineConfiguration.device.get() == m_device.get());

    const vk::Device& device = **m_device;

    cleanup();

    GraphicsPipelineConfiguration pipelineConfig(graphicsPipelineConfiguration);

    vk::Result result;

    std::vector<vk::DynamicState> dynamicStates = {};

    for (auto it = pipelineConfig.dynamicStates.begin(); it != pipelineConfig.dynamicStates.end(); ++it)
        if (it->second)
            dynamicStates.emplace_back(it->first);

    vk::Viewport viewport = pipelineConfig.viewport;
    if (viewport.width < 1 || viewport.height < 1) {
        viewport.width = (float)Engine::graphics()->getResolution().x;
        viewport.height = (float)Engine::graphics()->getResolution().y;
    }
    if (viewport.maxDepth <= viewport.minDepth) {
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
    }

    LOG_INFO("Recreating graphics pipeline \"%s\" [%g x %g]", name.c_str(), viewport.width, viewport.height);

    vk::FrontFace frontFace = pipelineConfig.frontFace;

    vk::Rect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = (uint32_t)viewport.width;
    scissor.extent.height = (uint32_t)viewport.height;

    viewport = GraphicsPipeline::getScreenViewport(viewport);
    if (Application::instance()->isViewportInverted())
        frontFace = frontFace == vk::FrontFace::eClockwise ? vk::FrontFace::eCounterClockwise : frontFace = vk::FrontFace::eClockwise;


    if (!pipelineConfig.vertexShader.has_value()) {
        LOG_ERROR("Vertex shader is required by a graphics pipeline, but was not supplied");
        return false;
    }

//    if (!pipelineConfig.fragmentShader.has_value()) {
//        printf("Fragment shader is required by a graphics pipeline, but was not supplied\n");
//        return false;
//    }

    if (pipelineConfig.renderPass.expired() || pipelineConfig.renderPass.get() == nullptr) {
        LOG_ERROR("Unable to create graphics pipeline with NULL RenderPass");
        return false;
    }

    std::vector<vk::ShaderModule> allShaderModules;
    allShaderModules.reserve(10); // Reallocation will break the references kept

    auto cleanupShaderModules = [&allShaderModules, &device]() {
        for (const auto& shaderModule : allShaderModules)
            device.destroyShaderModule(shaderModule);
    };

    // pipelineConfig is a copy
    Util::trim(pipelineConfig.vertexShaderEntryPoint);
    Util::trim(pipelineConfig.fragmentShaderEntryPoint);

    m_renderPass.set(pipelineConfig.renderPass, name + "-RenderPass");

    std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;

    if (pipelineConfig.vertexShader.has_value()) {
        if (pipelineConfig.vertexShaderEntryPoint.empty())
            pipelineConfig.vertexShaderEntryPoint = "main";
        vk::ShaderModule& vertexShaderModule = allShaderModules.emplace_back(nullptr);
        if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_VertexShader, device, pipelineConfig.vertexShader.value(), pipelineConfig.vertexShaderEntryPoint, &vertexShaderModule)) {
            cleanupShaderModules();
            return false;
        }
        vk::PipelineShaderStageCreateInfo& vertexShaderStageCreateInfo = pipelineShaderStages.emplace_back();
        vertexShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eVertex);
        vertexShaderStageCreateInfo.setModule(vertexShaderModule);
//        vertexShaderStageCreateInfo.setPName(vertexShaderEntryPoint.c_str());
        vertexShaderStageCreateInfo.setPName("main"); // For GLSL, the entry point is redefined as "main" with a macro
        vertexShaderStageCreateInfo.setPSpecializationInfo(nullptr); // TODO
    }

    if (pipelineConfig.fragmentShader.has_value()) {
        if (pipelineConfig.fragmentShaderEntryPoint.empty())
            pipelineConfig.fragmentShaderEntryPoint = "main";
        vk::ShaderModule& fragmentShaderModule = allShaderModules.emplace_back(nullptr);
        if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_FragmentShader, device, pipelineConfig.fragmentShader.value(), pipelineConfig.fragmentShaderEntryPoint, &fragmentShaderModule)) {
            cleanupShaderModules();
            return false;
        }
        vk::PipelineShaderStageCreateInfo& fragmentShaderStageCreateInfo = pipelineShaderStages.emplace_back();
        fragmentShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eFragment);
        fragmentShaderStageCreateInfo.setModule(fragmentShaderModule);
//        fragmentShaderStageCreateInfo.setPName(fragmentShaderEntryPoint.c_str());
        fragmentShaderStageCreateInfo.setPName("main");
        fragmentShaderStageCreateInfo.setPSpecializationInfo(nullptr); // TODO
    }

    vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.setVertexBindingDescriptions(pipelineConfig.vertexInputBindings);
    vertexInputStateCreateInfo.setVertexAttributeDescriptions(pipelineConfig.vertexInputAttributes);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.setTopology(pipelineConfig.primitiveTopology);
    inputAssemblyStateCreateInfo.setPrimitiveRestartEnable(false);

    vk::PipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.setViewportCount(1);
    viewportStateCreateInfo.setPViewports(&viewport);
    viewportStateCreateInfo.setScissorCount(1);
    viewportStateCreateInfo.setPScissors(&scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.setDepthClampEnable(false);
    rasterizationStateCreateInfo.setRasterizerDiscardEnable(false);
    rasterizationStateCreateInfo.setPolygonMode(pipelineConfig.polygonMode);
    rasterizationStateCreateInfo.setCullMode(pipelineConfig.cullMode);
    rasterizationStateCreateInfo.setFrontFace(frontFace);
    rasterizationStateCreateInfo.setDepthBiasEnable(pipelineConfig.depthBiasEnable);
    rasterizationStateCreateInfo.setDepthBiasConstantFactor(pipelineConfig.depthBias.constant);
    rasterizationStateCreateInfo.setDepthBiasClamp(pipelineConfig.depthBias.clamp);
    rasterizationStateCreateInfo.setDepthBiasSlopeFactor(pipelineConfig.depthBias.slope);
    rasterizationStateCreateInfo.setLineWidth(1.0F);

    vk::PipelineRasterizationLineStateCreateInfoEXT lineRasterizationLineStateCreateInfo;
//    lineRasterizationLineStateCreateInfo.stippledLineEnable = true;
    // TODO: line rasterization
    rasterizationStateCreateInfo.setPNext(&lineRasterizationLineStateCreateInfo);

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    multisampleStateCreateInfo.setSampleShadingEnable(false);
    multisampleStateCreateInfo.setMinSampleShading(1.0F);
    multisampleStateCreateInfo.setPSampleMask(nullptr);
    multisampleStateCreateInfo.setAlphaToCoverageEnable(false);
    multisampleStateCreateInfo.setAlphaToOneEnable(false);

    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
    depthStencilStateCreateInfo.setDepthTestEnable(pipelineConfig.depthTestEnabled);
    depthStencilStateCreateInfo.setDepthWriteEnable(pipelineConfig.depthWriteEnabled);
    depthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencilStateCreateInfo.setDepthBoundsTestEnable(false);
    depthStencilStateCreateInfo.setMinDepthBounds(0.0F);
    depthStencilStateCreateInfo.setMaxDepthBounds(1.0F);
    depthStencilStateCreateInfo.setStencilTestEnable(false);
    //depthStencilStateCreateInfo.front;
    //depthStencilStateCreateInfo.back;

    while ((uint32_t)pipelineConfig.attachmentBlendStates.size() < m_renderPass->getColourAttachmentCount(graphicsPipelineConfiguration.subpass))
        pipelineConfig.attachmentBlendStates.emplace_back(); // Expand attachmentBlendStates array until it matches the size of the render pass attachments

    std::vector<vk::PipelineColorBlendAttachmentState> attachmentBlendStates;
    for (const auto& blendState : pipelineConfig.attachmentBlendStates) {
        vk::PipelineColorBlendAttachmentState& colourBlendAttachmentState = attachmentBlendStates.emplace_back();
        colourBlendAttachmentState.setBlendEnable(blendState.blendEnable);
        colourBlendAttachmentState.setSrcColorBlendFactor(blendState.colourBlendMode.src);
        colourBlendAttachmentState.setDstColorBlendFactor(blendState.colourBlendMode.dst);
        colourBlendAttachmentState.setColorBlendOp(blendState.colourBlendMode.op);
        colourBlendAttachmentState.setSrcAlphaBlendFactor(blendState.alphaBlendMode.src);
        colourBlendAttachmentState.setDstAlphaBlendFactor(blendState.alphaBlendMode.dst);
        colourBlendAttachmentState.setAlphaBlendOp(blendState.alphaBlendMode.op);
        colourBlendAttachmentState.setColorWriteMask(blendState.colourWriteMask);
    }

    std::array<float, 4> blendConstants = {
            pipelineConfig.blendConstants.r,
            pipelineConfig.blendConstants.g,
            pipelineConfig.blendConstants.b,
            pipelineConfig.blendConstants.a
    };

    vk::PipelineColorBlendStateCreateInfo colourBlendStateCreateInfo;
    colourBlendStateCreateInfo.setLogicOpEnable(false);
    colourBlendStateCreateInfo.setLogicOp(vk::LogicOp::eCopy);
    colourBlendStateCreateInfo.setAttachments(attachmentBlendStates);
    colourBlendStateCreateInfo.setBlendConstants(blendConstants);

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    dynamicStateCreateInfo.setDynamicStates(dynamicStates);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setSetLayouts(pipelineConfig.descriptorSetLayouts);
    pipelineLayoutCreateInfo.setPushConstantRanges(pipelineConfig.pushConstantRanges);

    result = device.createPipelineLayout(&pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Unable to create GraphicsPipeline: Failed to create PipelineLayout: %s", vk::to_string(result).c_str());
        cleanupShaderModules();
        cleanup();
        return false;
    }

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.setStages(pipelineShaderStages);
    graphicsPipelineCreateInfo.setPVertexInputState(&vertexInputStateCreateInfo);
    graphicsPipelineCreateInfo.setPInputAssemblyState(&inputAssemblyStateCreateInfo);
    graphicsPipelineCreateInfo.setPTessellationState(nullptr); // TODO
    graphicsPipelineCreateInfo.setPViewportState(&viewportStateCreateInfo);
    graphicsPipelineCreateInfo.setPRasterizationState(&rasterizationStateCreateInfo);
    graphicsPipelineCreateInfo.setPMultisampleState(&multisampleStateCreateInfo);
    graphicsPipelineCreateInfo.setPDepthStencilState(&depthStencilStateCreateInfo); // TODO
    graphicsPipelineCreateInfo.setPColorBlendState(&colourBlendStateCreateInfo);
    graphicsPipelineCreateInfo.setPDynamicState(&dynamicStateCreateInfo);
    graphicsPipelineCreateInfo.setLayout(m_pipelineLayout);
    graphicsPipelineCreateInfo.setRenderPass(m_renderPass->getRenderPass());
    graphicsPipelineCreateInfo.setSubpass(graphicsPipelineConfiguration.subpass);
    graphicsPipelineCreateInfo.setBasePipelineHandle(nullptr);
    graphicsPipelineCreateInfo.setBasePipelineIndex(-1);

    bool doAbort = Engine::graphics()->doAbortOnVulkanError();
    Engine::graphics()->setAbortOnVulkanError(false);
    auto createGraphicsPipelineResult = device.createGraphicsPipeline(nullptr, graphicsPipelineCreateInfo);
    Engine::graphics()->setAbortOnVulkanError(doAbort);

    if (createGraphicsPipelineResult.result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create GraphicsPipeline: %s", vk::to_string(createGraphicsPipelineResult.result).c_str());
        cleanupShaderModules();
        cleanup();
        return false;
    }

    const vk::Pipeline& graphicsPipeline = createGraphicsPipelineResult.value;

    Engine::graphics()->setObjectName(device, (uint64_t)(VkPipeline)graphicsPipeline, vk::ObjectType::ePipeline, name);

    cleanupShaderModules();

    m_pipeline = graphicsPipeline;
    m_config = std::move(pipelineConfig);
    m_name = name;
    return true;
}

void GraphicsPipeline::bind(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}

const vk::Pipeline& GraphicsPipeline::getPipeline() const {
    return m_pipeline;
}

const SharedResource<RenderPass>& GraphicsPipeline::getRenderPass() const {
    return m_renderPass;
}

const vk::PipelineLayout& GraphicsPipeline::getPipelineLayout() const {
    return m_pipelineLayout;
}

const GraphicsPipelineConfiguration& GraphicsPipeline::getConfig() const {
    return m_config;
}

bool GraphicsPipeline::isValid() const {
    return !!m_pipeline && !!m_pipelineLayout && !!m_renderPass;
}

bool GraphicsPipeline::isStateDynamic(vk::DynamicState dynamicState) const {
    auto it = m_config.dynamicStates.find(dynamicState);
    return it != m_config.dynamicStates.end() && it->second;
}


void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const vk::Viewport* viewports) {
    validateDynamicState(vk::DynamicState::eViewport);
    commandBuffer.setViewport(firstViewport, viewportCount, viewports);
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const std::vector<vk::Viewport>& viewports) {
    setViewport(commandBuffer, firstViewport, (uint32_t)viewports.size(), viewports.data());
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const vk::Viewport& viewport) {
    setViewport(commandBuffer, firstViewport, 1, &viewport);
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, float width, float height, float x, float y, float minDepth, float maxDepth) {
    setViewport(commandBuffer, firstViewport, vk::Viewport(x, y, width, height, minDepth, maxDepth));
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, uint32_t firstViewport, const glm::vec2& size, const glm::vec2& offset, float minDepth, float maxDepth) {
    setViewport(commandBuffer, firstViewport, vk::Viewport(offset.x, offset.y, size.x, size.y, minDepth, maxDepth));
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const vk::Rect2D* scissorRects) {
    validateDynamicState(vk::DynamicState::eScissor);
    commandBuffer.setScissor(firstScissor, scissorCount, scissorRects);
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const std::vector<vk::Rect2D>& scissorRects) {
    setScissor(commandBuffer, firstScissor, (uint32_t)scissorRects.size(), scissorRects.data());
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const vk::Rect2D& scissorRect) {
    setScissor(commandBuffer, firstScissor, 1, &scissorRect);
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    setScissor(commandBuffer, firstScissor, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(width, height)));
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, uint32_t firstScissor, const glm::ivec2& offset, const glm::uvec2& size) {
    setScissor(commandBuffer, firstScissor, vk::Rect2D(vk::Offset2D(offset.x, offset.y), vk::Extent2D(size.x, size.y)));
}

void GraphicsPipeline::setLineWidth(const vk::CommandBuffer& commandBuffer, float lineWidth) {
    validateDynamicState(vk::DynamicState::eLineWidth);
    commandBuffer.setLineWidth(lineWidth);
}

void GraphicsPipeline::setDepthBias(const vk::CommandBuffer& commandBuffer, float constantFactor, float clamp, float slopeFactor) {
    validateDynamicState(vk::DynamicState::eDepthBias);
    commandBuffer.setDepthBias(constantFactor, clamp, slopeFactor);
}

void GraphicsPipeline::setBlendConstants(const vk::CommandBuffer& commandBuffer, const glm::vec4& constants) {
    validateDynamicState(vk::DynamicState::eBlendConstants);
    commandBuffer.setBlendConstants(&constants[0]);
}

void GraphicsPipeline::setBlendConstants(const vk::CommandBuffer& commandBuffer, float r, float g, float b, float a) {
    setBlendConstants(commandBuffer, glm::vec4(r, g, b, a));
}

void GraphicsPipeline::setDepthBounds(const vk::CommandBuffer& commandBuffer, float minDepthBound, float maxDepthBound) {
    validateDynamicState(vk::DynamicState::eDepthBounds);
    commandBuffer.setDepthBounds(minDepthBound, maxDepthBound);
}

void GraphicsPipeline::setStencilCompareMask(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t compareMask) {
    validateDynamicState(vk::DynamicState::eStencilCompareMask);
    commandBuffer.setStencilCompareMask(faceMask, compareMask);
}

void GraphicsPipeline::setStencilWriteMask(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t writeMask) {
    validateDynamicState(vk::DynamicState::eStencilWriteMask);
    commandBuffer.setStencilWriteMask(faceMask, writeMask);
}

void GraphicsPipeline::setStencilReference(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, uint32_t reference) {
    validateDynamicState(vk::DynamicState::eStencilReference);
    commandBuffer.setStencilReference(faceMask, reference);
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleLocationsInfoEXT& sampleLocations) {
    validateDynamicState(vk::DynamicState::eSampleLocationsEXT);
    commandBuffer.setSampleLocationsEXT(sampleLocations);
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, vk::SampleCountFlagBits samplesPerPixel, const vk::Extent2D& sampleGridSize, uint32_t sampleLocationsCount, const vk::SampleLocationEXT* sampleLocations) {
    setSampleLocations(commandBuffer, vk::SampleLocationsInfoEXT(samplesPerPixel, sampleGridSize, sampleLocationsCount, sampleLocations));
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, vk::SampleCountFlagBits samplesPerPixel, const vk::Extent2D& sampleGridSize, const std::vector<vk::SampleLocationEXT>& sampleLocations) {
    setSampleLocations(commandBuffer, samplesPerPixel, sampleGridSize, (uint32_t)sampleLocations.size(), sampleLocations.data());
}

void GraphicsPipeline::setLineStipple(const vk::CommandBuffer& commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    validateDynamicState(vk::DynamicState::eLineStippleEXT);
    commandBuffer.setLineStippleEXT(lineStippleFactor, lineStipplePattern);
}

void GraphicsPipeline::setCullMode(const vk::CommandBuffer& commandBuffer, vk::CullModeFlags cullMode) {
    validateDynamicState(vk::DynamicState::eCullModeEXT);
    commandBuffer.setCullModeEXT(cullMode);
}

void GraphicsPipeline::setFrontFace(const vk::CommandBuffer& commandBuffer, vk::FrontFace frontFace) {
    validateDynamicState(vk::DynamicState::eCullModeEXT);
    commandBuffer.setFrontFaceEXT(frontFace);
}

void GraphicsPipeline::setPrimitiveTopology(const vk::CommandBuffer& commandBuffer, vk::PrimitiveTopology primitiveTopology) {
    validateDynamicState(vk::DynamicState::ePrimitiveTopologyEXT);
    commandBuffer.setPrimitiveTopologyEXT(primitiveTopology);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets) {
    commandBuffer.bindVertexBuffers(firstBinding, bindingCount, buffers, offsets);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides) {
    validateDynamicState(vk::DynamicState::eVertexInputBindingStrideEXT);
    commandBuffer.bindVertexBuffers2EXT(firstBinding, bindingCount, buffers, offsets, sizes, strides);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data(), sizes.empty() ? nullptr : sizes.data(), strides.empty() ? nullptr : strides.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets) {
    std::vector<vk::Buffer> vkBuffers(bindingCount, nullptr);
    for (uint32_t i = 0; i < bindingCount; ++i)
        if (buffers[i] != nullptr) vkBuffers[i] = buffers[i]->getBuffer();
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)vkBuffers.size(), vkBuffers.data(), offsets);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, uint32_t bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides) {
    std::vector<vk::Buffer> vkBuffers(bindingCount, nullptr);
    for (uint32_t i = 0; i < bindingCount; ++i)
        if (buffers[i] != nullptr) vkBuffers[i] = buffers[i]->getBuffer();
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)vkBuffers.size(), vkBuffers.data(), offsets, sizes, strides);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, uint32_t firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data(), sizes.empty() ? nullptr : sizes.data(), strides.empty() ? nullptr : strides.data());
}


void GraphicsPipeline::setDepthTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eDepthTestEnableEXT);
    commandBuffer.setDepthTestEnableEXT(enabled);
}

void GraphicsPipeline::setDepthWriteEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eDepthWriteEnableEXT);
    commandBuffer.setDepthWriteEnableEXT(enabled);
}

void GraphicsPipeline::setDepthCompareOp(const vk::CommandBuffer& commandBuffer, vk::CompareOp compareOp) {
    validateDynamicState(vk::DynamicState::eDepthCompareOpEXT);
    commandBuffer.setDepthCompareOpEXT(compareOp);
}

void GraphicsPipeline::setDepthBoundsTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eDepthBoundsTestEnableEXT);
    commandBuffer.setDepthBoundsTestEnableEXT(enabled);
}

void GraphicsPipeline::setStencilTestEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eStencilTestEnableEXT);
    commandBuffer.setStencilTestEnableEXT(enabled);
}

void GraphicsPipeline::setStencilOp(const vk::CommandBuffer& commandBuffer, vk::StencilFaceFlags faceMask, vk::StencilOp failOp, vk::StencilOp passOp, vk::StencilOp depthFailOp, vk::CompareOp compareOp) {
    validateDynamicState(vk::DynamicState::eStencilOpEXT);
    commandBuffer.setStencilOpEXT(faceMask, failOp, passOp, depthFailOp, compareOp);
}

void GraphicsPipeline::setVertexInput(const vk::CommandBuffer& commandBuffer, uint32_t vertexBindingCount, const vk::VertexInputBindingDescription2EXT* vertexBindings, uint32_t vertexAttribCount, const vk::VertexInputAttributeDescription2EXT* vertexAttribs) {
    validateDynamicState(vk::DynamicState::eVertexInputEXT);
    commandBuffer.setVertexInputEXT(vertexBindingCount, vertexBindings, vertexAttribCount, vertexAttribs);
}

void GraphicsPipeline::setRasterizerDiscardEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eRasterizerDiscardEnableEXT);
    commandBuffer.setRasterizerDiscardEnableEXT(enabled);
}

void GraphicsPipeline::setDepthBiasEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eDepthBiasEnableEXT);
    commandBuffer.setDepthBiasEnableEXT(enabled);
}

void GraphicsPipeline::setLogicOp(const vk::CommandBuffer& commandBuffer, vk::LogicOp logicOp) {
    validateDynamicState(vk::DynamicState::eLogicOpEXT);
    commandBuffer.setLogicOpEXT(logicOp);
}

void GraphicsPipeline::setPrimitiveRestartEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::ePrimitiveRestartEnableEXT);
    commandBuffer.setPrimitiveRestartEnableEXT(enabled);
}

void GraphicsPipeline::setColourWriteEnabled(const vk::CommandBuffer& commandBuffer, bool enabled) {
    validateDynamicState(vk::DynamicState::eColorWriteEnableEXT);
    commandBuffer.setColorWriteEnableEXT(enabled);
}

void GraphicsPipeline::setPolygonMode(const vk::CommandBuffer& commandBuffer, vk::PolygonMode polygonMode) {
    validateDynamicState(vk::DynamicState::ePolygonModeEXT);
    commandBuffer.setPolygonModeEXT(polygonMode);
}

void GraphicsPipeline::setCullMode(const vk::CommandBuffer& commandBuffer, vk::CullModeFlagBits cullMode) {
    validateDynamicState(vk::DynamicState::eCullMode);
    commandBuffer.setCullMode(cullMode);
}

vk::Viewport GraphicsPipeline::getScreenViewport(const vk::Viewport& viewport) {
    if (Application::instance()->isViewportInverted())
        return vk::Viewport(viewport.x, viewport.y + viewport.height, viewport.width, viewport.height * -1, viewport.minDepth, viewport.maxDepth);
    return viewport;
}

vk::Viewport GraphicsPipeline::getScreenViewport(float width, float height, float x, float y, float minDepth, float maxDepth) {
    return getScreenViewport(vk::Viewport(x, y, width, height, minDepth, maxDepth));
}

vk::Viewport GraphicsPipeline::getScreenViewport(const glm::vec2& size, const glm::vec2& offset, float minDepth, float maxDepth) {
    return getScreenViewport(vk::Viewport(offset.x, offset.y, size.x, size.y, minDepth, maxDepth));
}

void GraphicsPipeline::cleanup() {
    (**m_device).destroyPipelineLayout(m_pipelineLayout);
    (**m_device).destroyPipeline(m_pipeline);
    m_pipelineLayout = nullptr;
    m_pipeline = nullptr;
    m_renderPass.reset();
}

void GraphicsPipeline::validateDynamicState(vk::DynamicState dynamicState) {
#if _DEBUG || 1
    if (!isStateDynamic(dynamicState)) {
        LOG_FATAL("Cannot set immutable pipeline state %s for pipeline \"%s\"", vk::to_string(dynamicState).c_str(), m_name.c_str());
        assert(false);
        return;
    }
#endif
}

#if ENABLE_SHADER_HOT_RELOAD

void GraphicsPipeline::onShaderLoaded(ShaderLoadedEvent* event) {
    bool doRecreate = false;

    if (m_config.vertexShader.has_value() && m_config.vertexShader.value() == event->filePath && m_config.vertexShaderEntryPoint == event->entryPoint) {
        LOG_INFO("Vertex shader %s for GraphicsPipeline %s", event->reloaded ? "reloaded" : "loaded", m_name.c_str());
        doRecreate = event->reloaded;
    }

    if (m_config.fragmentShader.has_value() && m_config.fragmentShader.value() == event->filePath && m_config.fragmentShaderEntryPoint == event->entryPoint) {
        LOG_INFO("Fragment shader %s for GraphicsPipeline %s", event->reloaded ? "reloaded" : "loaded", m_name.c_str());
        doRecreate = event->reloaded;
    }

    if (doRecreate) {
        Engine::graphics()->flushRendering([&event, this]() {
            // Backup current status in case recreate fails. It will always clean up the current resources, so we must exchange them with null
            vk::Pipeline backupPipeline = std::exchange(m_pipeline, nullptr);
            vk::PipelineLayout backupPipelineLayout = std::exchange(m_pipelineLayout, nullptr);
            SharedResource<RenderPass> backupRenderPass = std::exchange(m_renderPass, nullptr);
            GraphicsPipelineConfiguration backupConfig(m_config); // Copy

            bool success = recreate(m_config, m_name);
            if (!success) {
                LOG_WARN("Shader %s@%s was reloaded, but reconstructing GraphicsPipeline \"%s\" failed. The pipeline will remain unchanged", event->filePath.c_str(), event->entryPoint.c_str(), m_name.c_str());
                m_pipeline = backupPipeline;
                m_pipelineLayout = backupPipelineLayout;
                m_renderPass = std::move(backupRenderPass);
                m_config = backupConfig;
            } else {
                // We exchanged the old resources, so they were not cleaned up by recreate. We must destroy them up manually.
                (**m_device).destroyPipelineLayout(backupPipelineLayout);
                (**m_device).destroyPipeline(backupPipeline);
            }
        });
    }
}

#endif