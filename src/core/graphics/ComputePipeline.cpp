#include <utility>

#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ShaderUtils.h"
#include "core/application/Engine.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"
#include "core/util/Logger.h"


std::unordered_map<size_t, ComputePipeline*> ComputePipeline::s_cachedComputePipelines;


void ComputePipelineConfiguration::addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout) {
    assert(descriptorSetLayout);
    descriptorSetLayouts.emplace_back(descriptorSetLayout);
}

void ComputePipelineConfiguration::addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout) {
    assert(descriptorSetLayout != nullptr);
    addDescriptorSetLayout(descriptorSetLayout->getDescriptorSetLayout());
}

void ComputePipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& p_descriptorSetLayouts) {
    descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : p_descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void ComputePipelineConfiguration::setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& p_descriptorSetLayouts) {
    descriptorSetLayouts.clear();
    for (const auto& descriptorSetLayout : p_descriptorSetLayouts)
        addDescriptorSetLayout(descriptorSetLayout);
}

void ComputePipelineConfiguration::addPushConstantRange(const vk::PushConstantRange& pushConstantRange) {
    pushConstantRanges.emplace_back(pushConstantRange);
}

void ComputePipelineConfiguration::addPushConstantRange(vk::ShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    addPushConstantRange(vk::PushConstantRange(stageFlags, offset, size));
}

ComputePipeline::ComputePipeline(const WeakResource<vkr::Device>& device, const std::string& name):
        GraphicsResource(ResourceType_ComputePipeline, device, name) {

#if ENABLE_SHADER_HOT_RELOAD
    Engine::eventDispatcher()->connect(&ComputePipeline::onShaderLoaded, this);
#endif
}

ComputePipeline::~ComputePipeline() {
#if ENABLE_SHADER_HOT_RELOAD
    Engine::eventDispatcher()->disconnect(&ComputePipeline::onShaderLoaded, this);
#endif
    cleanup();
}

ComputePipeline* ComputePipeline::create(const WeakResource<vkr::Device>& device, const std::string& name) {
    return new ComputePipeline(device, name);
}

ComputePipeline* ComputePipeline::create(const ComputePipelineConfiguration& computePipelineConfiguration, const std::string& name) {
    ComputePipeline* computePipeline = create(computePipelineConfiguration.device, name);

    if (!computePipeline->recreate(computePipelineConfiguration, name)) {
        delete computePipeline;
        return nullptr;
    }

    return computePipeline;
}

ComputePipeline* ComputePipeline::getComputePipeline(const ComputePipelineConfiguration& computePipelineConfiguration, const std::string& name) {
    size_t hash = std::hash<ComputePipelineConfiguration>()(computePipelineConfiguration);
    auto it = s_cachedComputePipelines.find(hash);
    if (it == s_cachedComputePipelines.end()) {
        ComputePipeline* computePipeline = ComputePipeline::create(computePipelineConfiguration, name);
        if (computePipeline != nullptr)
            s_cachedComputePipelines.insert(std::make_pair(hash, computePipeline));
        return computePipeline;
    } else {
        return it->second;
    }
}

bool ComputePipeline::recreate(const ComputePipelineConfiguration& computePipelineConfiguration, const std::string& name) {
    const vk::Device& device = **m_device;
    vk::Result result;

    ComputePipelineConfiguration pipelineConfig(computePipelineConfiguration);

    cleanup();

    Util::trim(pipelineConfig.computeShaderEntryPoint);
    if (pipelineConfig.computeShaderEntryPoint.empty())
        pipelineConfig.computeShaderEntryPoint = "main";

    vk::ShaderModule computeShaderModule = nullptr;
    if (!ShaderUtils::loadShaderModule(ShaderUtils::ShaderStage_ComputeShader, device, pipelineConfig.computeShader, pipelineConfig.computeShaderEntryPoint, &computeShaderModule)) {
        LOG_ERROR("Unable to create ComputePipeline: Failed to load shader module");
        return false;
    }

    vk::PipelineShaderStageCreateInfo computeShaderStageCreateInfo{};
    computeShaderStageCreateInfo.setStage(vk::ShaderStageFlagBits::eCompute);
    computeShaderStageCreateInfo.setModule(computeShaderModule);
//    computeShaderStageCreateInfo.setPName(computeShaderEntryPoint.c_str());
    computeShaderStageCreateInfo.setPName("main");
    computeShaderStageCreateInfo.setPSpecializationInfo(nullptr); // TODO

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setSetLayouts(pipelineConfig.descriptorSetLayouts);
    pipelineLayoutCreateInfo.setPushConstantRanges(pipelineConfig.pushConstantRanges);

    vk::PipelineLayout pipelineLayout = nullptr;
    result = device.createPipelineLayout(&pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Unable to create ComputePipeline: Failed to create PipelineLayout: %s", vk::to_string(result).c_str());
        device.destroyShaderModule(computeShaderModule);
        cleanup();
        return false;
    }

    vk::ComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.setStage(computeShaderStageCreateInfo);
    pipelineCreateInfo.setLayout(m_pipelineLayout);

    auto createComputePipelineResult = device.createComputePipeline(nullptr, pipelineCreateInfo);
    if (createComputePipelineResult.result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create ComputePipeline: %s", vk::to_string(createComputePipelineResult.result).c_str());
        device.destroyShaderModule(computeShaderModule);
        cleanup();
        return false;
    }

    const vk::Pipeline& computePipeline = createComputePipelineResult.value;
    Engine::graphics()->setObjectName(device, (uint64_t)(VkPipeline)computePipeline, vk::ObjectType::ePipeline, name);

    device.destroyShaderModule(computeShaderModule);

    m_pipeline = computePipeline;
    m_config = std::move(pipelineConfig);
    m_name = name;
    return true;
}

void ComputePipeline::bind(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
}

void ComputePipeline::dispatch(const vk::CommandBuffer& commandBuffer, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) const {
    PROFILE_BEGIN_GPU_CMD("ComputePipeline::dispatch", commandBuffer);
    commandBuffer.dispatch(workgroupCountX, workgroupCountY, workgroupCountZ);
    PROFILE_END_GPU_CMD("ComputePipeline::dispatch", commandBuffer);
}

void ComputePipeline::dispatchBase(const vk::CommandBuffer& commandBuffer, uint32_t workgroupOffsetX, uint32_t workgroupOffsetY, uint32_t workgroupOffsetZ, uint32_t workgroupCountX, uint32_t workgroupCountY, uint32_t workgroupCountZ) const {
    PROFILE_BEGIN_GPU_CMD("ComputePipeline::dispatchBase", commandBuffer);
    commandBuffer.dispatchBase(workgroupOffsetX, workgroupOffsetY, workgroupOffsetZ, workgroupCountX, workgroupCountY, workgroupCountZ);
    PROFILE_END_GPU_CMD("ComputePipeline::dispatchBase", commandBuffer);
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
    m_pipelineLayout = nullptr;
    m_pipeline = nullptr;
}


#if ENABLE_SHADER_HOT_RELOAD

void ComputePipeline::onShaderLoaded(ShaderLoadedEvent* event) {
    bool doRecreate = false;

    if (m_config.computeShader == event->filePath && m_config.computeShaderEntryPoint == event->entryPoint) {
        LOG_INFO("Compute shader %s for ComputePipeline %s", event->reloaded ? "reloaded" : "loaded", m_name.c_str());
        doRecreate = event->reloaded;
    }

    if (doRecreate) {
        Engine::graphics()->flushRendering([&event, this]() {

            // Backup current status in case recreate fails. It will always clean up the current resources, so we must exchange them with null
            vk::Pipeline backupPipeline = std::exchange(m_pipeline, nullptr);
            vk::PipelineLayout backupPipelineLayout = std::exchange(m_pipelineLayout, nullptr);
            ComputePipelineConfiguration backupConfig(m_config); // Copy

            bool success = recreate(m_config, m_name);
            if (!success) {
                LOG_WARN("Shader %s@%s was reloaded, but reconstructing ComputePipeline \"%s\" failed. The pipeline will remain unchanged", event->filePath.c_str(), event->entryPoint.c_str(), m_name.c_str());
                m_pipeline = backupPipeline;
                m_pipelineLayout = backupPipelineLayout;
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