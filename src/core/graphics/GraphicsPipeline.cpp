#include "core/graphics/GraphicsPipeline.h"
#include "core/application/Application.h"
#include "core/application/Engine.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/ShaderUtils.h"


AttachmentBlendState::AttachmentBlendState(const bool& blendEnable, const vk::ColorComponentFlags& colourWriteMask):
    blendEnable(blendEnable),
    colourWriteMask(colourWriteMask) {
}

AttachmentBlendState::AttachmentBlendState(const bool& blendEnable, const uint32_t& colourWriteMask):
    AttachmentBlendState(blendEnable, (vk::ColorComponentFlags)colourWriteMask) {
}

void AttachmentBlendState::setColourBlendMode(const BlendMode& blendMode) {
    colourBlendMode = blendMode;
}

void AttachmentBlendState::setColourBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op) {
    colourBlendMode.src = src;
    colourBlendMode.dst = dst;
    colourBlendMode.op = op;
}


void AttachmentBlendState::setAlphaBlendMode(const BlendMode& blendMode) {
    alphaBlendMode = blendMode;
}

void AttachmentBlendState::setAlphaBlendMode(const vk::BlendFactor& src, const vk::BlendFactor& dst, const vk::BlendOp& op) {
    alphaBlendMode.src = src;
    alphaBlendMode.dst = dst;
    alphaBlendMode.op = op;
}



void GraphicsPipelineConfiguration::setViewport(const vk::Viewport& viewport) {
    this->viewport = viewport;
}

void GraphicsPipelineConfiguration::setViewport(const glm::vec2& size, const glm::vec2& offset, const float& minDepth, const float& maxDepth) {
    setViewport(vk::Viewport(offset.x, offset.y, size.x, size.y, minDepth, maxDepth));
}

void GraphicsPipelineConfiguration::setViewport(const float& width, const float& height, const float& x, const float& y, const float& minDepth, const float& maxDepth) {
    setViewport(vk::Viewport(x, y, width, height, minDepth, maxDepth));
}

void GraphicsPipelineConfiguration::addVertexInputBinding(const vk::VertexInputBindingDescription& vertexInputBinding) {
    vertexInputBindings.emplace_back(vertexInputBinding);
}

void GraphicsPipelineConfiguration::addVertexInputBinding(const uint32_t& binding, const uint32_t& stride, const vk::VertexInputRate& vertexInputRate) {
    addVertexInputBinding(vk::VertexInputBindingDescription(binding, stride, vertexInputRate));
}

void GraphicsPipelineConfiguration::setVertexInputBindings(const vk::ArrayProxy<const vk::VertexInputBindingDescription>& vertexInputBindings) {
    this->vertexInputBindings.clear();
    for (const auto& vertexInputBinding : vertexInputBindings)
        addVertexInputBinding(vertexInputBinding);
}

void GraphicsPipelineConfiguration::addVertexInputAttribute(const vk::VertexInputAttributeDescription& vertexInputAttribute) {
    vertexInputAttributes.emplace_back(vertexInputAttribute);
}

void GraphicsPipelineConfiguration::addVertexInputAttribute(const uint32_t& location, const uint32_t& binding, const vk::Format& format, const uint32_t& offset) {
    addVertexInputAttribute(vk::VertexInputAttributeDescription(location, binding, format, offset));
}

void GraphicsPipelineConfiguration::setVertexInputAttribute(const vk::ArrayProxy<const vk::VertexInputAttributeDescription>& vertexInputAttributes) {
    this->vertexInputAttributes.clear();
    for (const auto& vertexInputAttribute : vertexInputAttributes)
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

void GraphicsPipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& descriptorSetLayouts) {
    this->descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void GraphicsPipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& descriptorSetLayouts) {
    this->descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void GraphicsPipelineConfiguration::setDynamicState(const vk::DynamicState& dynamicState, const bool& isDynamic) {
    dynamicStates[dynamicState] = isDynamic;
}

void GraphicsPipelineConfiguration::setDynamicStates(const vk::ArrayProxy<const vk::DynamicState>& dynamicStates, const bool& isDynamic) {
    for (const auto& state : dynamicStates)
        setDynamicState(state, isDynamic);
}

void GraphicsPipelineConfiguration::setAttachmentBlendState(const size_t& attachmentIndex, const AttachmentBlendState& attachmentBlendState) {
    while (attachmentBlendStates.size() <= attachmentIndex)
        attachmentBlendStates.emplace_back();
    attachmentBlendStates[attachmentIndex] = attachmentBlendState;
}

GraphicsPipeline::GraphicsPipeline(std::weak_ptr<vkr::Device> device):
        m_device(device) {
}

GraphicsPipeline::GraphicsPipeline(std::weak_ptr<vkr::Device> device,
                                   vk::Pipeline& pipeline,
                                   vk::PipelineLayout& pipelineLayout,
                                   std::shared_ptr<RenderPass>& renderPass,
                                   const GraphicsPipelineConfiguration& config):
        m_device(std::move(device)),
        m_pipeline(std::move(pipeline)),
        m_renderPass(std::move(renderPass)),
        m_pipelineLayout(std::move(pipelineLayout)),
        m_config(config) {
}

GraphicsPipeline::~GraphicsPipeline() {
    cleanup();
}

GraphicsPipeline* GraphicsPipeline::create(std::weak_ptr<vkr::Device> device) {
    return new GraphicsPipeline(device.lock());
}

GraphicsPipeline* GraphicsPipeline::create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const char* name) {
    GraphicsPipeline* graphicsPipeline = create(graphicsPipelineConfiguration.device);

    if (!graphicsPipeline->recreate(graphicsPipelineConfiguration, name)) {
        delete graphicsPipeline;
        return nullptr;
    }

    return graphicsPipeline;
}

bool GraphicsPipeline::recreate(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration, const char* name) {
    assert(!graphicsPipelineConfiguration.device.expired() && graphicsPipelineConfiguration.device.lock() == m_device);

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

    printf("Recreating graphics pipeline [%f x %f]\n", viewport.width, viewport.height);


    vk::FrontFace frontFace = pipelineConfig.frontFace;

    vk::Rect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = (uint32_t)viewport.width;
    scissor.extent.height = (uint32_t)viewport.height;

    if (Application::instance()->isViewportInverted()) {
        viewport.y += viewport.height;
        viewport.height *= -1;
        if (frontFace == vk::FrontFace::eClockwise)
            frontFace = vk::FrontFace::eCounterClockwise;
        else
            frontFace = vk::FrontFace::eClockwise;
    }


    if (!pipelineConfig.vertexShader.has_value()) {
        printf("Vertex shader is required by a graphics pipeline, but was not supplied\n");
        return false;
    }

//    if (!pipelineConfig.fragmentShader.has_value()) {
//        printf("Fragment shader is required by a graphics pipeline, but was not supplied\n");
//        return false;
//    }

    if (pipelineConfig.renderPass.expired() || pipelineConfig.renderPass.lock() == nullptr) {
        printf("Unable to create graphics pipeline with NULL RenderPass\n");
        return false;
    }

    std::vector<vk::ShaderModule> allShaderModules;
    allShaderModules.reserve(10); // Reallocation will break the references kept

    auto cleanupShaderModules = [&allShaderModules, &device]() {
        for (const auto& shaderModule : allShaderModules)
            device.destroyShaderModule(shaderModule);
    };



    m_renderPass = std::shared_ptr<RenderPass>(pipelineConfig.renderPass);

    std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;

    if (pipelineConfig.vertexShader.has_value()) {
        vk::ShaderModule &vertexShaderModule = allShaderModules.emplace_back(VK_NULL_HANDLE);
        if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_VertexShader, device, pipelineConfig.vertexShader.value(), &vertexShaderModule)) {
            cleanupShaderModules();
            return false;
        }
        vk::PipelineShaderStageCreateInfo &vertexShaderStageCreateInfo = pipelineShaderStages.emplace_back();
        vertexShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eVertex);
        vertexShaderStageCreateInfo.setModule(vertexShaderModule);
        vertexShaderStageCreateInfo.setPName("main");
        vertexShaderStageCreateInfo.setPSpecializationInfo(nullptr); // TODO
    }

    if (pipelineConfig.fragmentShader.has_value()) {
        vk::ShaderModule &fragmentShaderModule = allShaderModules.emplace_back(VK_NULL_HANDLE);
        if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_FragmentShader, device, pipelineConfig.fragmentShader.value(), &fragmentShaderModule)) {
            cleanupShaderModules();
            return false;
        }
        vk::PipelineShaderStageCreateInfo &fragmentShaderStageCreateInfo = pipelineShaderStages.emplace_back();
        fragmentShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eFragment);
        fragmentShaderStageCreateInfo.setModule(fragmentShaderModule);
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
    rasterizationStateCreateInfo.setPNext(&lineRasterizationLineStateCreateInfo);
//    lineRasterizationLineStateCreateInfo.stippledLineEnable = true;

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    multisampleStateCreateInfo.setSampleShadingEnable(false);
    multisampleStateCreateInfo.setMinSampleShading(1.0F);
    multisampleStateCreateInfo.setPSampleMask(nullptr);
    multisampleStateCreateInfo.setAlphaToCoverageEnable(false);
    multisampleStateCreateInfo.setAlphaToOneEnable(false);

    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo;
    depthStencilStateCreateInfo.setDepthTestEnable(true);
    depthStencilStateCreateInfo.setDepthWriteEnable(true);
    depthStencilStateCreateInfo.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencilStateCreateInfo.setDepthBoundsTestEnable(false);
    depthStencilStateCreateInfo.setMinDepthBounds(0.0F);
    depthStencilStateCreateInfo.setMaxDepthBounds(1.0F);
    depthStencilStateCreateInfo.setStencilTestEnable(false);
    //depthStencilStateCreateInfo.front;
    //depthStencilStateCreateInfo.back;

    while (pipelineConfig.attachmentBlendStates.size() < m_renderPass->getColourAttachmentCount())
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

    vk::PipelineColorBlendStateCreateInfo colourBlendStateCreateInfo;
    colourBlendStateCreateInfo.setLogicOpEnable(false);
    colourBlendStateCreateInfo.setLogicOp(vk::LogicOp::eCopy);
    colourBlendStateCreateInfo.setAttachments(attachmentBlendStates);
    colourBlendStateCreateInfo.setBlendConstants({ 0.0F, 0.0F, 0.0F, 0.0F });

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    dynamicStateCreateInfo.setDynamicStates(dynamicStates);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setSetLayouts(pipelineConfig.descriptorSetLayouts);
    pipelineLayoutCreateInfo.setPushConstantRangeCount(0);

    result = device.createPipelineLayout(&pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != vk::Result::eSuccess) {
        printf("Unable to create GraphicsPipeline: Failed to create PipelineLayout: %s\n", vk::to_string(result).c_str());
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
    graphicsPipelineCreateInfo.setBasePipelineHandle(VK_NULL_HANDLE);
    graphicsPipelineCreateInfo.setBasePipelineIndex(-1);

    auto createGraphicsPipelineResult = device.createGraphicsPipeline(VK_NULL_HANDLE, graphicsPipelineCreateInfo);
    if (createGraphicsPipelineResult.result != vk::Result::eSuccess) {
        printf("Failed to create GraphicsPipeline: %s\n", vk::to_string(createGraphicsPipelineResult.result).c_str());
        cleanupShaderModules();
        cleanup();
        return false;
    }

    const vk::Pipeline& graphicsPipeline = createGraphicsPipelineResult.value;

    Engine::graphics()->setObjectName(device, (uint64_t)(VkPipeline)graphicsPipeline, vk::ObjectType::ePipeline, name);

    cleanupShaderModules();

    m_pipeline = graphicsPipeline;
    m_config = std::move(pipelineConfig);
    return true;
}

void GraphicsPipeline::bind(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
}

const vk::Pipeline& GraphicsPipeline::getPipeline() const {
    return m_pipeline;
}

std::shared_ptr<RenderPass> GraphicsPipeline::getRenderPass() const {
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

bool GraphicsPipeline::isStateDynamic(const vk::DynamicState& dynamicState) const {
    auto it = m_config.dynamicStates.find(dynamicState);
    return it != m_config.dynamicStates.end() && it->second;
}


void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const uint32_t& viewportCount, const vk::Viewport* viewports) {
    validateDynamicState(vk::DynamicState::eViewport);
    commandBuffer.setViewport(firstViewport, viewportCount, viewports);
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const std::vector<vk::Viewport>& viewports) {
    setViewport(commandBuffer, firstViewport, (uint32_t)viewports.size(), viewports.data());
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const vk::Viewport& viewport) {
    setViewport(commandBuffer, firstViewport, 1, &viewport);
}

void GraphicsPipeline::setViewport(const vk::CommandBuffer& commandBuffer, const uint32_t& firstViewport, const float& x, const float& y, const float& width, const float& height, const float& minDepth, const float& maxDepth) {
    setViewport(commandBuffer, firstViewport, vk::Viewport(x, y, width, height, minDepth, maxDepth));
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const uint32_t& scissorCount, const vk::Rect2D* scissorRects) {
    validateDynamicState(vk::DynamicState::eScissor);
    commandBuffer.setScissor(firstScissor, scissorCount, scissorRects);
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const std::vector<vk::Rect2D>& scissorRects) {
    setScissor(commandBuffer, firstScissor, (uint32_t)scissorRects.size(), scissorRects.data());
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const vk::Rect2D& scissorRect) {
    setScissor(commandBuffer, firstScissor, 1, &scissorRect);
}

void GraphicsPipeline::setScissor(const vk::CommandBuffer& commandBuffer, const uint32_t& firstScissor, const int32_t& x, const int32_t& y, const uint32_t& width, const uint32_t& height) {
    setScissor(commandBuffer, firstScissor, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(width, height)));
}

void GraphicsPipeline::setLineWidth(const vk::CommandBuffer& commandBuffer, const float& lineWidth) {
    validateDynamicState(vk::DynamicState::eLineWidth);
    commandBuffer.setLineWidth(lineWidth);
}

void GraphicsPipeline::setDepthBias(const vk::CommandBuffer& commandBuffer, const float& constantFactor, const float& clamp, const float& slopeFactor) {
    validateDynamicState(vk::DynamicState::eDepthBias);
    commandBuffer.setDepthBias(constantFactor, clamp, slopeFactor);
}

void GraphicsPipeline::setBlendConstants(const vk::CommandBuffer& commandBuffer, const glm::vec4& constants) {
    validateDynamicState(vk::DynamicState::eBlendConstants);
    commandBuffer.setBlendConstants(&constants[0]);
}

void GraphicsPipeline::setBlendConstants(const vk::CommandBuffer& commandBuffer, const float& r, const float& g, const float& b, const float& a) {
    setBlendConstants(commandBuffer, glm::vec4(r, g, b, a));
}

void GraphicsPipeline::setDepthBounds(const vk::CommandBuffer& commandBuffer, const float& minDepthBound, const float& maxDepthBound) {
    validateDynamicState(vk::DynamicState::eDepthBounds);
    commandBuffer.setDepthBounds(minDepthBound, maxDepthBound);
}

void GraphicsPipeline::setStencilCompareMask(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& compareMask) {
    validateDynamicState(vk::DynamicState::eStencilCompareMask);
    commandBuffer.setStencilCompareMask(faceMask, compareMask);
}

void GraphicsPipeline::setStencilWriteMask(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& writeMask) {
    validateDynamicState(vk::DynamicState::eStencilWriteMask);
    commandBuffer.setStencilWriteMask(faceMask, writeMask);
}

void GraphicsPipeline::setStencilReference(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const uint32_t& reference) {
    validateDynamicState(vk::DynamicState::eStencilReference);
    commandBuffer.setStencilReference(faceMask, reference);
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleLocationsInfoEXT& sampleLocations) {
    validateDynamicState(vk::DynamicState::eSampleLocationsEXT);
    commandBuffer.setSampleLocationsEXT(sampleLocations);
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleCountFlagBits& samplesPerPixel, const vk::Extent2D& sampleGridSize, const uint32_t& sampleLocationsCount, const vk::SampleLocationEXT* sampleLocations) {
    setSampleLocations(commandBuffer, vk::SampleLocationsInfoEXT(samplesPerPixel, sampleGridSize, sampleLocationsCount, sampleLocations));
}

void GraphicsPipeline::setSampleLocations(const vk::CommandBuffer& commandBuffer, const vk::SampleCountFlagBits& samplesPerPixel, const vk::Extent2D& sampleGridSize, const std::vector<vk::SampleLocationEXT>& sampleLocations) {
    setSampleLocations(commandBuffer, samplesPerPixel, sampleGridSize, (uint32_t)sampleLocations.size(), sampleLocations.data());
}

void GraphicsPipeline::setLineStipple(const vk::CommandBuffer& commandBuffer, const uint32_t& lineStippleFactor, const uint16_t& lineStipplePattern) {
    validateDynamicState(vk::DynamicState::eLineStippleEXT);
    commandBuffer.setLineStippleEXT(lineStippleFactor, lineStipplePattern);
}

void GraphicsPipeline::setCullMode(const vk::CommandBuffer& commandBuffer, const vk::CullModeFlags& cullMode) {
    validateDynamicState(vk::DynamicState::eCullModeEXT);
    commandBuffer.setCullModeEXT(cullMode);
}

void GraphicsPipeline::setFrontFace(const vk::CommandBuffer& commandBuffer, const vk::FrontFace& frontFace) {
    validateDynamicState(vk::DynamicState::eCullModeEXT);
    commandBuffer.setFrontFaceEXT(frontFace);
}

void GraphicsPipeline::setPrimitiveTopology(const vk::CommandBuffer& commandBuffer, const vk::PrimitiveTopology& primitiveTopology) {
    validateDynamicState(vk::DynamicState::ePrimitiveTopologyEXT);
    commandBuffer.setPrimitiveTopologyEXT(primitiveTopology);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets) {
    commandBuffer.bindVertexBuffers(firstBinding, bindingCount, buffers, offsets);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, const vk::Buffer* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides) {
    validateDynamicState(vk::DynamicState::eVertexInputBindingStrideEXT);
    commandBuffer.bindVertexBuffers2EXT(firstBinding, bindingCount, buffers, offsets, sizes, strides);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<vk::Buffer>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data(), sizes.empty() ? nullptr : sizes.data(), strides.empty() ? nullptr : strides.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets) {
    std::vector<vk::Buffer> vkBuffers(bindingCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < bindingCount; ++i)
        if (buffers[i] != nullptr) vkBuffers[i] = buffers[i]->getBuffer();
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)vkBuffers.size(), vkBuffers.data(), offsets);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data());
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const uint32_t& bindingCount, Buffer* const* buffers, const vk::DeviceSize* offsets, const vk::DeviceSize* sizes, const vk::DeviceSize* strides) {
    std::vector<vk::Buffer> vkBuffers(bindingCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < bindingCount; ++i)
        if (buffers[i] != nullptr) vkBuffers[i] = buffers[i]->getBuffer();
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)vkBuffers.size(), vkBuffers.data(), offsets, sizes, strides);
}

void GraphicsPipeline::bindVertexBuffers(const vk::CommandBuffer& commandBuffer, const uint32_t& firstBinding, const std::vector<Buffer*>& buffers, const std::vector<vk::DeviceSize>& offsets, const std::vector<vk::DeviceSize>& sizes, const std::vector<vk::DeviceSize>& strides) {
    bindVertexBuffers(commandBuffer, firstBinding, (uint32_t)buffers.size(), buffers.data(), offsets.data(), sizes.empty() ? nullptr : sizes.data(), strides.empty() ? nullptr : strides.data());
}



void GraphicsPipeline::setDepthTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eDepthTestEnableEXT);
    commandBuffer.setDepthTestEnableEXT(enabled);
}

void GraphicsPipeline::setDepthWriteEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eDepthWriteEnableEXT);
    commandBuffer.setDepthWriteEnableEXT(enabled);
}

void GraphicsPipeline::setDepthCompareOp(const vk::CommandBuffer& commandBuffer, const vk::CompareOp& compareOp) {
    validateDynamicState(vk::DynamicState::eDepthCompareOpEXT);
    commandBuffer.setDepthCompareOpEXT(compareOp);
}

void GraphicsPipeline::setDepthBoundsTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eDepthBoundsTestEnableEXT);
    commandBuffer.setDepthBoundsTestEnableEXT(enabled);
}

void GraphicsPipeline::setStencilTestEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eStencilTestEnableEXT);
    commandBuffer.setStencilTestEnableEXT(enabled);
}

void GraphicsPipeline::setStencilOp(const vk::CommandBuffer& commandBuffer, const vk::StencilFaceFlags& faceMask, const vk::StencilOp& failOp, const vk::StencilOp& passOp, const vk::StencilOp& depthFailOp, const vk::CompareOp& compareOp) {
    validateDynamicState(vk::DynamicState::eStencilOpEXT);
    commandBuffer.setStencilOpEXT(faceMask, failOp, passOp, depthFailOp, compareOp);
}

void GraphicsPipeline::setVertexInput(const vk::CommandBuffer& commandBuffer, const uint32_t& vertexBindingCount, const vk::VertexInputBindingDescription2EXT* vertexBindings, const uint32_t& vertexAttribCount, const vk::VertexInputAttributeDescription2EXT* vertexAttribs) {
    validateDynamicState(vk::DynamicState::eVertexInputEXT);
    commandBuffer.setVertexInputEXT(vertexBindingCount, vertexBindings, vertexAttribCount, vertexAttribs);
}

void GraphicsPipeline::setRasterizerDiscardEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eRasterizerDiscardEnableEXT);
    commandBuffer.setRasterizerDiscardEnableEXT(enabled);
}

void GraphicsPipeline::setDepthBiasEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eDepthBiasEnableEXT);
    commandBuffer.setDepthBiasEnableEXT(enabled);
}

void GraphicsPipeline::setLogicOp(const vk::CommandBuffer& commandBuffer, const vk::LogicOp& logicOp) {
    validateDynamicState(vk::DynamicState::eLogicOpEXT);
    commandBuffer.setLogicOpEXT(logicOp);
}

void GraphicsPipeline::setPrimitiveRestartEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::ePrimitiveRestartEnableEXT);
    commandBuffer.setPrimitiveRestartEnableEXT(enabled);
}

void GraphicsPipeline::setColourWriteEnabled(const vk::CommandBuffer& commandBuffer, const bool& enabled) {
    validateDynamicState(vk::DynamicState::eColorWriteEnableEXT);
    commandBuffer.setColorWriteEnableEXT(enabled);
}

void GraphicsPipeline::cleanup() {
    (**m_device).destroyPipelineLayout(m_pipelineLayout);
    (**m_device).destroyPipeline(m_pipeline);
    m_pipelineLayout = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
    m_renderPass.reset();
}

void GraphicsPipeline::validateDynamicState(const vk::DynamicState& dynamicState) {
#if _DEBUG || 1
    if (!isStateDynamic(dynamicState)) {
        printf("Cannot set immutable pipeline state: %s\n", vk::to_string(dynamicState).c_str());
        assert(false);
        return;
    }
#endif
}