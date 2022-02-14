#include "GraphicsPipeline.h"
#include "../application/Application.h"
#include "GraphicsManager.h"
#include <fstream>
#include <filesystem>

#define GLSL_COMPILER_EXECUTABLE "D:/Code/VulkanSDK/1.2.198.1/Bin/glslc.exe"

GraphicsPipeline::GraphicsPipeline(std::weak_ptr<vkr::Device> device,
	std::unique_ptr<vkr::Pipeline>& pipeline,
	std::unique_ptr<vkr::RenderPass>& renderPass,
	std::unique_ptr<vkr::PipelineLayout>& pipelineLayout):
	m_device(std::move(device)),
	m_pipeline(std::move(pipeline)),
	m_renderPass(std::move(renderPass)),
	m_pipelineLayout(std::move(pipelineLayout)) {
}

GraphicsPipeline::~GraphicsPipeline() {

}

GraphicsPipeline* GraphicsPipeline::create(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
	assert(!graphicsPipelineConfiguration.device.expired());

	const vkr::Device& device = *graphicsPipelineConfiguration.device.lock();

	VkResult result;

	std::vector<vk::DynamicState> dynamicStates = {};

	vk::Viewport viewport;
	viewport.x = 0.0F;
	viewport.y = 0.0F;
	viewport.width = graphicsPipelineConfiguration.framebufferWidth == 0 ? Application::instance()->graphics()->getResolution().x : graphicsPipelineConfiguration.framebufferWidth;
	viewport.height = graphicsPipelineConfiguration.framebufferHeight == 0 ? Application::instance()->graphics()->getResolution().y : graphicsPipelineConfiguration.framebufferHeight;
	viewport.minDepth = 0.0F;
	viewport.maxDepth = 1.0F;

	vk::Rect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = viewport.width;
	scissor.extent.height = viewport.height;

	if (!graphicsPipelineConfiguration.vertexShader.has_value()) {
		printf("Vertex shader is required by a graphics pipeline, but was not supplied\n");
		return NULL;
	}

	if (!graphicsPipelineConfiguration.fragmentShader.has_value()) {
		printf("Fragment shader is required by a graphics pipeline, but was not supplied\n");
		return NULL;
	}

	std::vector<char> shaderBytecode;
	vk::ShaderModuleCreateInfo shaderModuleCreateInfo;

	if (!loadShaderStage(graphicsPipelineConfiguration.vertexShader.value(), shaderBytecode)) {
		printf("Failed to create vertex shader module for shader file \"%s\"\n", graphicsPipelineConfiguration.vertexShader.value().c_str());
		return NULL;
	}
	shaderModuleCreateInfo.setCodeSize(shaderBytecode.size());
	shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(shaderBytecode.data()));
	vkr::ShaderModule vertexShaderModule = device.createShaderModule(shaderModuleCreateInfo);

	if (!loadShaderStage(graphicsPipelineConfiguration.fragmentShader.value(), shaderBytecode)) {
		printf("Failed to create fragment shader module for shader file \"%s\"\n", graphicsPipelineConfiguration.fragmentShader.value().c_str());
		return NULL;
	}
	shaderModuleCreateInfo.setCodeSize(shaderBytecode.size());
	shaderModuleCreateInfo.setPCode(reinterpret_cast<const uint32_t*>(shaderBytecode.data()));
	vkr::ShaderModule fragmentShaderModule = device.createShaderModule(shaderModuleCreateInfo);


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
	inputAssemblyStateCreateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
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
	rasterizationStateCreateInfo.setFrontFace(graphicsPipelineConfiguration.frontFace);
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

	std::unique_ptr<vkr::PipelineLayout> pipelineLayout = std::make_unique<vkr::PipelineLayout>(device, pipelineLayoutCreateInfo);

	std::vector<vk::AttachmentDescription> renderPassAttachments;
	vk::AttachmentDescription& colourAttachment = renderPassAttachments.emplace_back();
	colourAttachment.setFormat(Application::instance()->graphics()->getColourFormat());
	colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
	colourAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
	colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
	colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
	colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
	colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentDescription& depthAttachment = renderPassAttachments.emplace_back();
	depthAttachment.setFormat(Application::instance()->graphics()->getDepthFormat());
	depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
	depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
	depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
	depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
	depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
	depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	std::vector<std::vector<vk::AttachmentReference>> allSubpassColourAttachments;
	std::vector<vk::SubpassDescription> subpasses;

	std::vector<vk::AttachmentReference>& subpassColourAttachments = allSubpassColourAttachments.emplace_back();
	vk::AttachmentReference& colourAttachmentReference = subpassColourAttachments.emplace_back();
	colourAttachmentReference.setAttachment(0);
	colourAttachmentReference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthStencilAttachmentReference;
	depthStencilAttachmentReference.setAttachment(1);
	depthStencilAttachmentReference.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	vk::SubpassDescription& subpassDescription = subpasses.emplace_back();
	subpassDescription.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	subpassDescription.setColorAttachments(subpassColourAttachments);
	subpassDescription.setPResolveAttachments(NULL);
	subpassDescription.setPDepthStencilAttachment(&depthStencilAttachmentReference);
	subpassDescription.setPreserveAttachmentCount(0);
	subpassDescription.setPPreserveAttachments(NULL);

	std::vector<vk::SubpassDependency> subpassDependencies;
	vk::SubpassDependency& subpassDependency = subpassDependencies.emplace_back();
	subpassDependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
	subpassDependency.setDstSubpass(0);
	subpassDependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
	subpassDependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
	subpassDependency.setSrcAccessMask({});
	subpassDependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

	vk::RenderPassCreateInfo renderPassCreateInfo;
	renderPassCreateInfo.setAttachments(renderPassAttachments);
	renderPassCreateInfo.setSubpasses(subpasses);
	renderPassCreateInfo.setDependencies(subpassDependencies);

	std::unique_ptr<vkr::RenderPass> renderPass = std::make_unique<vkr::RenderPass>(device, renderPassCreateInfo);

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
	graphicsPipelineCreateInfo.setLayout(**pipelineLayout);
	graphicsPipelineCreateInfo.setRenderPass(**renderPass);
	graphicsPipelineCreateInfo.setSubpass(0);
	graphicsPipelineCreateInfo.setBasePipelineHandle(VK_NULL_HANDLE);
	graphicsPipelineCreateInfo.setBasePipelineIndex(-1);

	std::unique_ptr<vkr::Pipeline> graphicsPipeline = std::make_unique<vkr::Pipeline>(device, VK_NULL_HANDLE, graphicsPipelineCreateInfo);

	GraphicsPipeline* pipeline = new GraphicsPipeline(graphicsPipelineConfiguration.device, graphicsPipeline, renderPass, pipelineLayout);
	return pipeline;
}

void GraphicsPipeline::bind(const vk::CommandBuffer& commandBuffer) {
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, **m_pipeline);
}

const vk::Pipeline& GraphicsPipeline::getPipeline() const {
	return **m_pipeline;
}

const vk::RenderPass& GraphicsPipeline::getRenderPass() const {
	return **m_renderPass;
}

const vk::PipelineLayout& GraphicsPipeline::getPipelineLayout() const {
	return **m_pipelineLayout;
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
#if defined(__WIN32__) defined(_WIN32) || defined(_WIN64)
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