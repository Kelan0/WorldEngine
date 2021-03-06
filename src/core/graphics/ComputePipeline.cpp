#include "core/graphics/ComputePipeline.h"
#include "core/graphics/ShaderUtils.h"
#include "core/graphics/DescriptorSet.h"


std::unordered_map<size_t, ComputePipeline*> ComputePipeline::s_cachedComputePipelines;


void ComputePipelineConfiguration::addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout) {
    assert(descriptorSetLayout);
    descriptorSetLayouts.emplace_back(descriptorSetLayout);
}

void ComputePipelineConfiguration::addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout) {
    assert(descriptorSetLayout != nullptr);
    addDescriptorSetLayout(descriptorSetLayout->getDescriptorSetLayout());
}

void ComputePipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& descriptorSetLayouts) {
    this->descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void ComputePipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& descriptorSetLayouts) {
    this->descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void ComputePipelineConfiguration::addPushConstantRange(const vk::PushConstantRange& pushConstantRange) {
    pushConstantRanges.emplace_back(pushConstantRange);
}

void ComputePipelineConfiguration::addPushConstantRange(const vk::ShaderStageFlags& stageFlags, const uint32_t& offset, const uint32_t& size) {
    addPushConstantRange(vk::PushConstantRange(stageFlags, offset, size));
}

ComputePipeline::ComputePipeline(std::weak_ptr<vkr::Device> device):
    m_device(device) {
}

ComputePipeline::ComputePipeline(std::weak_ptr<vkr::Device> device,
                                   vk::Pipeline& pipeline,
                                   vk::PipelineLayout& pipelineLayout,
                                   const ComputePipelineConfiguration& config):
        m_device(std::move(device)),
        m_pipeline(std::move(pipeline)),
        m_pipelineLayout(std::move(pipelineLayout)),
        m_config(config) {
}

ComputePipeline::~ComputePipeline() {
    cleanup();
}

ComputePipeline* ComputePipeline::create(std::weak_ptr<vkr::Device> device) {
    return new ComputePipeline(device.lock());
}

ComputePipeline* ComputePipeline::create(const ComputePipelineConfiguration& computePipelineConfiguration) {
    ComputePipeline* computePipeline = create(computePipelineConfiguration.device);

    if (!computePipeline->recreate(computePipelineConfiguration)) {
        delete computePipeline;
        return NULL;
    }

    return computePipeline;
}

ComputePipeline* ComputePipeline::getComputePipeline(const ComputePipelineConfiguration& computePipelineConfiguration) {
    size_t hash = std::hash<ComputePipelineConfiguration>()(computePipelineConfiguration);
    auto it = s_cachedComputePipelines.find(hash);
    if (it == s_cachedComputePipelines.end()) {
        ComputePipeline* computePipeline = ComputePipeline::create(computePipelineConfiguration);
        if (computePipeline != nullptr)
            s_cachedComputePipelines.insert(std::make_pair(hash, computePipeline));
        return computePipeline;
    } else {
        return it->second;
    }
}

bool ComputePipeline::recreate(const ComputePipelineConfiguration& computePipelineConfiguration) {

    const vk::Device& device = **m_device;
    vk::Result result;

    cleanup();

    vk::ShaderModule computeShaderModule = VK_NULL_HANDLE;
    if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_ComputeShader, device, computePipelineConfiguration.computeShader, &computeShaderModule)) {
        printf("Unable to create ComputePipeline: Failed to load shader module\n");
        return false;
    }

    vk::PipelineShaderStageCreateInfo computeShaderStageCreateInfo{};
    computeShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eCompute);
    computeShaderStageCreateInfo.setModule(computeShaderModule);
    computeShaderStageCreateInfo.setPName(computePipelineConfiguration.computeStageEntryFunctionName.c_str());
    computeShaderStageCreateInfo.setPSpecializationInfo(NULL); // TODO

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setSetLayouts(computePipelineConfiguration.descriptorSetLayouts);
    pipelineLayoutCreateInfo.setPushConstantRangeCount(0);
    pipelineLayoutCreateInfo.setPushConstantRanges(computePipelineConfiguration.pushConstantRanges);

    result = device.createPipelineLayout(&pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != vk::Result::eSuccess) {
        printf("Unable to create ComputePipeline: Failed to create PipelineLayout: %s\n", vk::to_string(result).c_str());
        device.destroyShaderModule(computeShaderModule);
        return false;
    }

    vk::ComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.setStage(computeShaderStageCreateInfo);
    pipelineCreateInfo.setLayout(m_pipelineLayout);

    auto createComputePipelineResult = device.createComputePipeline(VK_NULL_HANDLE, pipelineCreateInfo);
    if (createComputePipelineResult.result != vk::Result::eSuccess) {
        printf("Failed to create ComputePipeline: %s\n", vk::to_string(createComputePipelineResult.result).c_str());
        device.destroyShaderModule(computeShaderModule);
        return false;
    }

    device.destroyShaderModule(computeShaderModule);

    m_pipeline = createComputePipelineResult.value;
    m_config = std::move(computePipelineConfiguration);
    return true;
}

void ComputePipeline::bind(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
}

void ComputePipeline::dispatch(const vk::CommandBuffer& commandBuffer, const uint32_t& workgroupCountX, const uint32_t& workgroupCountY, const uint32_t& workgroupCountZ) const {
    commandBuffer.dispatch(workgroupCountX, workgroupCountY, workgroupCountZ);
}

void ComputePipeline::dispatchBase(const vk::CommandBuffer& commandBuffer, const uint32_t& workgroupOffsetX, const uint32_t& workgroupOffsetY, const uint32_t& workgroupOffsetZ, const uint32_t& workgroupCountX, const uint32_t& workgroupCountY, const uint32_t& workgroupCountZ) const {
    commandBuffer.dispatchBase(workgroupOffsetX, workgroupOffsetY, workgroupOffsetZ, workgroupCountX, workgroupCountY, workgroupCountZ);
}

const vk::Pipeline& ComputePipeline::getPipeline() const {
    return m_pipeline;
}

const vk::PipelineLayout& ComputePipeline::getPipelineLayout() const {
    return m_pipelineLayout;
}

const ComputePipelineConfiguration& ComputePipeline::getConfig() const {
    return m_config;
}

bool ComputePipeline::isValid() const {
    return !!m_pipeline && !!m_pipelineLayout;
}

void ComputePipeline::cleanup() {
    (**m_device).destroyPipelineLayout(m_pipelineLayout);
    (**m_device).destroyPipeline(m_pipeline);
    m_pipelineLayout = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
}
