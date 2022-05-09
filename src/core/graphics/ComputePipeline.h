#ifndef WORLDENGINE_COMPUTEPIPELINE_H
#define WORLDENGINE_COMPUTEPIPELINE_H

#include "core/core.h"

#define ALWAYS_RELOAD_SHADERS

class Buffer;
class DescriptorSetLayout;


struct ComputePipelineConfiguration {
    std::weak_ptr<vkr::Device> device;
    std::string computeShader;
    std::string computeStageEntryFunctionName = "main";
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
    std::vector<vk::PushConstantRange> pushConstantRanges;

    void addDescriptorSetLayout(const vk::DescriptorSetLayout& descriptorSetLayout);

    void addDescriptorSetLayout(const DescriptorSetLayout* descriptorSetLayout);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const vk::DescriptorSetLayout>& descriptorSetLayouts);

    void setDescriptorSetLayouts(const vk::ArrayProxy<const DescriptorSetLayout*>& descriptorSetLayouts);

    void addPushConstantRange(const vk::PushConstantRange& pushConstantRange);

    void addPushConstantRange(const vk::ShaderStageFlags& stageFlags, const uint32_t& offset, const uint32_t& size);
};

class ComputePipeline {
    NO_COPY(ComputePipeline);

private:
    ComputePipeline(std::weak_ptr<vkr::Device> device);

    ComputePipeline(std::weak_ptr<vkr::Device> device,
                     vk::Pipeline& pipeline,
                     vk::PipelineLayout& pipelineLayout,
                     const ComputePipelineConfiguration& config);

public:
    ~ComputePipeline();

    static ComputePipeline* create(std::weak_ptr<vkr::Device> device);

    static ComputePipeline* create(const ComputePipelineConfiguration& computePipelineConfiguration);

    static ComputePipeline* getComputePipeline(const ComputePipelineConfiguration& computePipelineConfiguration);

    bool recreate(const ComputePipelineConfiguration& computePipelineConfiguration);

    void bind(const vk::CommandBuffer& commandBuffer) const;

    void dispatch(const vk::CommandBuffer& commandBuffer, const uint32_t& workgroupCountX, const uint32_t& workgroupCountY, const uint32_t& workgroupCountZ) const;

    void dispatchBase(const vk::CommandBuffer& commandBuffer, const uint32_t& workgroupOffsetX, const uint32_t& workgroupOffsetY, const uint32_t& workgroupOffsetZ, const uint32_t& workgroupCountX, const uint32_t& workgroupCountY, const uint32_t& workgroupCountZ) const;

    const vk::Pipeline& getPipeline() const;

    const vk::PipelineLayout& getPipelineLayout() const;

    const ComputePipelineConfiguration& getConfig() const;

    bool isValid() const;

private:
    void cleanup();

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipelineLayout;
    ComputePipelineConfiguration m_config;

    static std::unordered_map<size_t, ComputePipeline*> s_cachedComputePipelines;
};




namespace std {
    template<>
    struct hash<ComputePipelineConfiguration> {
        size_t operator()(const ComputePipelineConfiguration& computePipelineConfiguration) const {
            size_t hash = 0;
            std::hash_combine(hash, computePipelineConfiguration.computeShader);
            std::hash_combine(hash, computePipelineConfiguration.computeStageEntryFunctionName);
            for (const auto &descriptorSetLayout : computePipelineConfiguration.descriptorSetLayouts)
                std::hash_combine(hash, (void*)((VkDescriptorSetLayout)descriptorSetLayout));
            return hash;
        }
    };
}



#endif //WORLDENGINE_COMPUTEPIPELINE_H
