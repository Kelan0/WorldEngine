#include "core/graphics/GraphicsPipeline.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include <fstream>
#include <filesystem>

#define GLSL_COMPILER_EXECUTABLE "D:/Code/VulkanSDK/1.2.198.1/Bin/glslc.exe"


void GraphicsPipelineConfiguration::setViewport(float width, float height, float x, float y, float minDepth, float maxDepth) {
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
}

void GraphicsPipelineConfiguration::setDynamicState(const vk::DynamicState& dynamicState, const bool& isDynamic) {
    dynamicStates[dynamicState] = isDynamic;
}

void GraphicsPipelineConfiguration::setDynamicStates(const std::vector<vk::DynamicState>& dynamicStates, const bool& isDynamic) {
    for (const auto& state : dynamicStates)
        setDynamicState(state, isDynamic);
}

GraphicsPipeline::GraphicsPipeline(std::weak_ptr<vkr::Device> device):
        m_device(device) {
}

GraphicsPipeline::GraphicsPipeline(std::weak_ptr<vkr::Device> device,
                                   std::unique_ptr<vkr::Pipeline>& pipeline,
                                   std::shared_ptr<RenderPass>& renderPass,
                                   std::unique_ptr<vkr::PipelineLayout>& pipelineLayout,
                                   const GraphicsPipelineConfiguration& config):
        m_device(std::move(device)),
        m_pipeline(std::move(pipeline)),
        m_renderPass(std::move(renderPass)),
        m_pipelineLayout(std::move(pipelineLayout)),
        m_config(config) {
}

GraphicsPipeline::~GraphicsPipeline() {

}

GraphicsPipeline* GraphicsPipeline::create(std::weak_ptr<vkr::Device> device) {
    return new GraphicsPipeline(device.lock());
}

GraphicsPipeline* GraphicsPipeline::create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
    GraphicsPipeline* graphicsPipeline = create(graphicsPipelineConfiguration.device);

    if (!graphicsPipeline->recreate(graphicsPipelineConfiguration)) {
        delete graphicsPipeline;
        return NULL;
    }

    return graphicsPipeline;
}

bool GraphicsPipeline::recreate(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
    assert(!graphicsPipelineConfiguration.device.expired() && graphicsPipelineConfiguration.device.lock() == m_device);

    VkResult result;

    std::vector<vk::DynamicState> dynamicStates = {};

    for (auto it = graphicsPipelineConfiguration.dynamicStates.begin(); it != graphicsPipelineConfiguration.dynamicStates.end(); ++it)
        if (it->second)
            dynamicStates.emplace_back(it->first);

    vk::Viewport viewport = graphicsPipelineConfiguration.viewport;
    if (viewport.width < 1 || viewport.height < 1) {
        viewport.width = (float)Application::instance()->graphics()->getResolution().x;
        viewport.height = (float)Application::instance()->graphics()->getResolution().y;
    }
    if (viewport.maxDepth <= viewport.minDepth) {
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;
    }

    printf("Recreating graphics pipeline [%f x %f]\n", viewport.width, viewport.height);


    vk::FrontFace frontFace = graphicsPipelineConfiguration.frontFace;

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


    if (!graphicsPipelineConfiguration.vertexShader.has_value()) {
        printf("Vertex shader is required by a graphics pipeline, but was not supplied\n");
        m_pipelineLayout.reset();
        m_renderPass.reset();
        m_pipeline.reset();
        return false;
    }

    if (!graphicsPipelineConfiguration.fragmentShader.has_value()) {
        printf("Fragment shader is required by a graphics pipeline, but was not supplied\n");
        m_pipelineLayout.reset();
        m_renderPass.reset();
        m_pipeline.reset();
        return false;
    }

    std::vector<char> shaderBytecode;
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo;

    if (!loadShaderStage(graphicsPipelineConfiguration.vertexShader.value(), shaderBytecode)) {
        printf("Failed to create vertex shader module for shader file \"%s\"\n", graphicsPipelineConfiguration.vertexShader.value().c_str());
        m_pipelineLayout.reset();
        m_renderPass.reset();
        m_pipeline.reset();
        return false;
    }
    shaderModuleCreateInfo.setCodeSize(shaderBytecode.size());
    shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(shaderBytecode.data()));
    vkr::ShaderModule vertexShaderModule = m_device->createShaderModule(shaderModuleCreateInfo);

    if (!loadShaderStage(graphicsPipelineConfiguration.fragmentShader.value(), shaderBytecode)) {
        printf("Failed to create fragment shader module for shader file \"%s\"\n", graphicsPipelineConfiguration.fragmentShader.value().c_str());
        m_pipelineLayout.reset();
        m_renderPass.reset();
        m_pipeline.reset();
        return false;
    }
    shaderModuleCreateInfo.setCodeSize(shaderBytecode.size());
    shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(shaderBytecode.data()));
    vkr::ShaderModule fragmentShaderModule = m_device->createShaderModule(shaderModuleCreateInfo);


    std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;

    vk::PipelineShaderStageCreateInfo& vertexShaderStageCreateInfo = pipelineShaderStages.emplace_back();
    vertexShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexShaderStageCreateInfo.setModule(*vertexShaderModule);
    vertexShaderStageCreateInfo.setPName("main");
    vertexShaderStageCreateInfo.setPSpecializationInfo(NULL); // TODO

    vk::PipelineShaderStageCreateInfo& fragmentShaderStageCreateInfo = pipelineShaderStages.emplace_back();
    fragmentShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentShaderStageCreateInfo.setModule(*fragmentShaderModule);
    fragmentShaderStageCreateInfo.setPName("main");
    fragmentShaderStageCreateInfo.setPSpecializationInfo(NULL); // TODO

    vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.setVertexBindingDescriptions(graphicsPipelineConfiguration.vertexInputBindings);
    vertexInputStateCreateInfo.setVertexAttributeDescriptions(graphicsPipelineConfiguration.vertexInputAttributes);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.setTopology(graphicsPipelineConfiguration.primitiveTopology);
    inputAssemblyStateCreateInfo.setPrimitiveRestartEnable(false);

    vk::PipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.setViewportCount(1);
    viewportStateCreateInfo.setPViewports(&viewport);
    viewportStateCreateInfo.setScissorCount(1);
    viewportStateCreateInfo.setPScissors(&scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.setDepthClampEnable(false);
    rasterizationStateCreateInfo.setRasterizerDiscardEnable(false);
    rasterizationStateCreateInfo.setPolygonMode(graphicsPipelineConfiguration.polygonMode);
    rasterizationStateCreateInfo.setCullMode(graphicsPipelineConfiguration.cullMode);
    rasterizationStateCreateInfo.setFrontFace(frontFace);
    rasterizationStateCreateInfo.setDepthBiasEnable(false);
    rasterizationStateCreateInfo.setDepthBiasConstantFactor(0.0F);
    rasterizationStateCreateInfo.setDepthBiasClamp(0.0F);
    rasterizationStateCreateInfo.setDepthBiasSlopeFactor(0.0F);
    rasterizationStateCreateInfo.setLineWidth(1.0F);

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    multisampleStateCreateInfo.setSampleShadingEnable(false);
    multisampleStateCreateInfo.setMinSampleShading(1.0F);
    multisampleStateCreateInfo.setPSampleMask(NULL);
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

    std::vector<vk::PipelineColorBlendAttachmentState> attachments;
    vk::PipelineColorBlendAttachmentState& colourBlendAttachmentState = attachments.emplace_back();
    colourBlendAttachmentState.setBlendEnable(false);
    colourBlendAttachmentState.setSrcColorBlendFactor(vk::BlendFactor::eOne);
    colourBlendAttachmentState.setDstColorBlendFactor(vk::BlendFactor::eZero);
    colourBlendAttachmentState.setColorBlendOp(vk::BlendOp::eAdd);
    colourBlendAttachmentState.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    colourBlendAttachmentState.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    colourBlendAttachmentState.setAlphaBlendOp(vk::BlendOp::eAdd);
    colourBlendAttachmentState.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colourBlendStateCreateInfo;
    colourBlendStateCreateInfo.setLogicOpEnable(false);
    colourBlendStateCreateInfo.setLogicOp(vk::LogicOp::eCopy);
    colourBlendStateCreateInfo.setAttachments(attachments);
    colourBlendStateCreateInfo.setBlendConstants({ 0.0F, 0.0F, 0.0F, 0.0F });

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    dynamicStateCreateInfo.setDynamicStates(dynamicStates);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setSetLayouts(graphicsPipelineConfiguration.descriptorSetLayous);
    pipelineLayoutCreateInfo.setPushConstantRangeCount(0);

    m_pipelineLayout = std::make_unique<vkr::PipelineLayout>(*m_device, pipelineLayoutCreateInfo);

//    RenderPassConfiguration renderPassConfig;
//    renderPassConfig.device = graphicsPipelineConfiguration.device;
//    vk::AttachmentDescription colourAttachment;
//    colourAttachment.setFormat(graphicsPipelineConfiguration.colourFormat);
//    colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
//    colourAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//    colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
//    colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//    colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
//    colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//    colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
//    renderPassConfig.addAttachment(colourAttachment);
//    vk::AttachmentDescription depthAttachment;
//    depthAttachment.setFormat(graphicsPipelineConfiguration.depthFormat);
//    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
//    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
//    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
//    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
//    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
//    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
//    depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
//    renderPassConfig.addAttachment(depthAttachment);
//    SubpassConfiguration subpassConfiguration;
//    subpassConfiguration.addColourAttachment(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));
//    subpassConfiguration.setDepthStencilAttachment(vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal));
//    renderPassConfig.addSubpass(subpassConfiguration);
//    vk::SubpassDependency subpassDependency;
//    subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
//    subpassDependency.setDstSubpass(0);
//    subpassDependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
//    subpassDependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
//    subpassDependency.setSrcAccessMask({});
//    subpassDependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);
//    renderPassConfig.addSubpassDependency(subpassDependency);
//
//    m_renderPass = std::shared_ptr<RenderPass>(RenderPass::create(renderPassConfig));

    m_renderPass = std::shared_ptr<RenderPass>(graphicsPipelineConfiguration.renderPass);

    vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.setStages(pipelineShaderStages);
    graphicsPipelineCreateInfo.setPVertexInputState(&vertexInputStateCreateInfo);
    graphicsPipelineCreateInfo.setPInputAssemblyState(&inputAssemblyStateCreateInfo);
    graphicsPipelineCreateInfo.setPTessellationState(NULL); // TODO
    graphicsPipelineCreateInfo.setPViewportState(&viewportStateCreateInfo);
    graphicsPipelineCreateInfo.setPRasterizationState(&rasterizationStateCreateInfo);
    graphicsPipelineCreateInfo.setPMultisampleState(&multisampleStateCreateInfo);
    graphicsPipelineCreateInfo.setPDepthStencilState(&depthStencilStateCreateInfo); // TODO
    graphicsPipelineCreateInfo.setPColorBlendState(&colourBlendStateCreateInfo);
    graphicsPipelineCreateInfo.setPDynamicState(&dynamicStateCreateInfo);
    graphicsPipelineCreateInfo.setLayout(**m_pipelineLayout);
    graphicsPipelineCreateInfo.setRenderPass(m_renderPass->getRenderPass());
    graphicsPipelineCreateInfo.setSubpass(0);
    graphicsPipelineCreateInfo.setBasePipelineHandle(VK_NULL_HANDLE);
    graphicsPipelineCreateInfo.setBasePipelineIndex(-1);

    m_pipeline = std::make_unique<vkr::Pipeline>(*m_device, VK_NULL_HANDLE, graphicsPipelineCreateInfo);
    m_config = graphicsPipelineConfiguration;
    return true;
}

void GraphicsPipeline::bind(const vk::CommandBuffer& commandBuffer) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, **m_pipeline);
}

const vk::Pipeline& GraphicsPipeline::getPipeline() const {
    return **m_pipeline;
}

std::shared_ptr<RenderPass> GraphicsPipeline::getRenderPass() const {
    return m_renderPass;
}

const vk::PipelineLayout& GraphicsPipeline::getPipelineLayout() const {
    return **m_pipelineLayout;
}

const GraphicsPipelineConfiguration& GraphicsPipeline::getConfig() const {
    return m_config;
}

bool GraphicsPipeline::isValid() const {
    return !!m_pipeline && !!m_pipelineLayout && !!m_renderPass;
}

bool GraphicsPipeline::loadShaderStage(std::string filePath, std::vector<char>& bytecode) {

    // TODO: determine if source file is GLSL or HLSL and call correct compiler

    if (!filePath.ends_with(".spv")) {

        std::string outputFilePath = filePath + ".spv";

        bool shouldCompile = false;

        if (!std::filesystem::exists(outputFilePath)) {
            // Compiled file does not exist.

            if (!std::filesystem::exists(filePath)) {
                // Source file does not exist
                printf("Shader source file \"%s\" was not found\n", filePath.c_str());
                return false;
            }

            printf("Compiling shader file \"%s\"\n", filePath.c_str());
            shouldCompile = true;

        } else if (std::filesystem::exists(filePath)) {
            // Compiled file and source file both exist

            auto lastModifiedSource = std::filesystem::last_write_time(filePath);
            auto lastCompiled = std::filesystem::last_write_time(outputFilePath);

            if (lastModifiedSource > lastCompiled) {
                // Source file was modified after the shader was last compiled
                printf("Recompiling shader file \"%s\"\n", filePath.c_str());
                shouldCompile = true;
            }
        }

        if (shouldCompile) {
            if (!runCommand(std::string(GLSL_COMPILER_EXECUTABLE) + " \"" + filePath + "\" -o \"" + outputFilePath + "\"")) {
                printf("Could not execute SPIR-V compile command. Provide a pre-compiled .spv file instead\n");
                return false;
            }
        }

        filePath = outputFilePath;
    }

    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        printf("Shader file \"%s\" was not found\n", filePath.c_str());
        return false;
    }

    bytecode.resize(file.tellg());
    file.seekg(0);
    file.read(bytecode.data(), bytecode.size());
    file.close();

    return true;
}

bool GraphicsPipeline::runCommand(const std::string& command) {
#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
    int error = system(command.c_str());
    if (error != 0) {
        return false;
    }

    return true;
#else
    printf("Unable to execute commands on unsupported platform\n");
#endif

    return false;
}