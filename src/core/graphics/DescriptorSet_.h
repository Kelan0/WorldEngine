#pragma once

#include "../core.h"

struct DescriptorAllocatorConfiguration {
	std::shared_ptr<vkr::Device> device;

	std::unordered_map<vk::DescriptorType, float> poolSizeFactors = {
		{ vk::DescriptorType::eSampler, 0.5F },
		{ vk::DescriptorType::eCombinedImageSampler, 4.0F },
		{ vk::DescriptorType::eSampledImage, 4.0F },
		{ vk::DescriptorType::eStorageImage, 1.0F },
		{ vk::DescriptorType::eUniformTexelBuffer, 1.0F },
		{ vk::DescriptorType::eStorageTexelBuffer, 1.0F },
		{ vk::DescriptorType::eUniformBuffer, 2.0F },
		{ vk::DescriptorType::eStorageBuffer, 2.0F },
		{ vk::DescriptorType::eUniformBufferDynamic, 1.0F },
		{ vk::DescriptorType::eStorageBufferDynamic, 1.0F },
		{ vk::DescriptorType::eInputAttachment, 0.5F }
	};
};

struct DescriptorLayoutCacheConfiguration {
	std::shared_ptr<vkr::Device> device;
};

class DescriptorAllocator {
	NO_COPY(DescriptorAllocator);
private:
	DescriptorAllocator(std::shared_ptr<vkr::Device> device, std::unordered_map<vk::DescriptorType, float> poolSizeFactors);

public:
	~DescriptorAllocator();

	static DescriptorAllocator* create(const DescriptorAllocatorConfiguration& descriptorAllocatorConfiguration);

	void resetPools();

	bool allocate(const vk::DescriptorSetLayout& descriptorSetLayout, std::shared_ptr<vkr::DescriptorSet>& outDescriptorSet);

private:
	uint32_t createPool(uint32_t maxSets, vk::DescriptorPoolCreateFlags flags);

	uint32_t grabPool();

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unordered_map<vk::DescriptorType, float> m_poolSizeFactors;

	std::vector<std::unique_ptr<vkr::DescriptorPool>> m_descriptorPools;
	std::unordered_map<vk::DescriptorPool, std::vector<std::shared_ptr<vkr::DescriptorSet>>> m_managedAllocatedSets;
	std::vector<uint32_t> m_usedPoolIndices;
	std::vector<uint32_t> m_freePoolIndices;
	uint32_t m_currentPoolIndex = UINT32_MAX;

	bool m_canFreeDescriptorSets;
};


class DescriptorLayoutCache {
	NO_COPY(DescriptorLayoutCache);
public:
	DescriptorLayoutCache(std::shared_ptr<vkr::Device> device);

	~DescriptorLayoutCache();

	std::shared_ptr<vkr::DescriptorSetLayout> getDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo);

private:
	struct DescriptorSetLayoutKey {
		uint32_t flags;
		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		bool operator==(const DescriptorSetLayoutKey& rhs) const;
	};

	struct DescriptorSetLayoutKeyHasher {
		size_t operator()(const DescriptorSetLayoutKey& descriptorSetLayoutKey) const;
	};

private:
	std::shared_ptr<vkr::Device> m_device;
	
	std::unordered_map<DescriptorSetLayoutKey, std::shared_ptr<vkr::DescriptorSetLayout>, DescriptorSetLayoutKeyHasher> m_layouts;
};


class DescriptorBuilder {
	NO_COPY(DescriptorBuilder);
public:
	DescriptorBuilder(DescriptorAllocator* allocator, DescriptorLayoutCache* layoutCache);

	~DescriptorBuilder();

	DescriptorBuilder& bindBuffers(uint32_t bindingIndex, const std::vector<vk::DescriptorBufferInfo>& descriptorBufferInfos, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags);

	DescriptorBuilder& bindBuffer(uint32_t bindingIndex, const vk::DescriptorBufferInfo& descriptorBufferInfo, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags);

	DescriptorBuilder& bindBuffer(uint32_t bindingIndex, vk::Buffer buffer, size_t offset, size_t range, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags);

	bool build(std::shared_ptr<vkr::DescriptorSet>& outDescriptorSet, std::shared_ptr<vkr::DescriptorSetLayout>& outDescriptorSetLayout);

	void reset();

private:
	DescriptorAllocator* m_allocator;
	DescriptorLayoutCache* m_layoutCache;
	std::vector<vk::DescriptorSetLayoutBinding> m_bindings;
	std::vector<vk::WriteDescriptorSet> m_writes;
	std::vector<vk::DescriptorBufferInfo> m_tempBuffers;
};