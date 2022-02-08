#include "DescriptorSet.h"

DescriptorAllocator::DescriptorAllocator(std::shared_ptr<vkr::Device> device, std::unordered_map<vk::DescriptorType, float> poolSizeFactors):
	m_device(std::move(device)),
	m_poolSizeFactors(poolSizeFactors) {

	m_canFreeDescriptorSets = true;
}

DescriptorAllocator::~DescriptorAllocator() {
	resetPools();
}

DescriptorAllocator* DescriptorAllocator::create(const DescriptorAllocatorConfiguration& descriptorAllocatorConfiguration) {
	// Probably the most unnecessary create method like this, but it sticks to the existing convention for other vulkan manager classes
	return new DescriptorAllocator(descriptorAllocatorConfiguration.device, descriptorAllocatorConfiguration.poolSizeFactors);
}

void DescriptorAllocator::resetPools() {
	for (int i = 0; i < m_usedPoolIndices.size(); ++i) {
		const vkr::DescriptorPool& pool = *m_descriptorPools[m_usedPoolIndices[i]];

		auto it = m_managedAllocatedSets.find(*pool);
		if (it != m_managedAllocatedSets.end()) {
			auto& allocatedSets = it->second;

#if _DEBUG
			for (int i = 0; i < allocatedSets.size(); ++i) {

				if (allocatedSets[i].use_count() > 1) {
					printf("Cannot reset descriptor pool while some allocated descriptor sets still have external references\n");
					assert(false);
					return;
				}
			}
#endif
			allocatedSets.clear();
			m_managedAllocatedSets.erase(it);
		}

		pool.reset();
	}

	m_usedPoolIndices.clear();
	m_freePoolIndices.clear();

	for (int i = 0; i < m_descriptorPools.size(); ++i) {
		m_freePoolIndices.push_back(i);
	}

	m_currentPoolIndex = UINT32_MAX;
}

bool DescriptorAllocator::allocate(const vk::DescriptorSetLayout& descriptorSetLayout, std::shared_ptr<vkr::DescriptorSet>& outDescriptorSet) {
	if (m_currentPoolIndex == UINT32_MAX) {
		m_currentPoolIndex = grabPool();

		if (m_currentPoolIndex == UINT32_MAX) {
			printf("Unable to allocate descriptor set. Failed to acquire a pool to allocate from\n");
			return false;
		}

		m_usedPoolIndices.push_back(m_currentPoolIndex);
	}


	vk::DescriptorSetAllocateInfo allocateInfo;
	allocateInfo.setDescriptorPool(**m_descriptorPools[m_currentPoolIndex]);
	allocateInfo.setDescriptorSetCount(1);
	allocateInfo.setPSetLayouts(&descriptorSetLayout);

	vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;
	vk::Result result = (**m_device).allocateDescriptorSets(&allocateInfo, &descriptorSet);

	if (result == vk::Result::eErrorFragmentedPool || result == vk::Result::eErrorOutOfPoolMemory) {
		m_currentPoolIndex = grabPool();
		m_usedPoolIndices.push_back(m_currentPoolIndex);

		allocateInfo.setDescriptorPool(**m_descriptorPools[m_currentPoolIndex]);
		result = (**m_device).allocateDescriptorSets(&allocateInfo, &descriptorSet);
	}

	if (result != vk::Result::eSuccess) {
		printf("Failed to allocate descriptor set\n");
		return false;
	}

	outDescriptorSet = std::make_shared<vkr::DescriptorSet>(*m_device, descriptorSet, allocateInfo.descriptorPool);

	if (!m_canFreeDescriptorSets) {
		auto& allocatedSets = m_managedAllocatedSets[allocateInfo.descriptorPool];
		allocatedSets.push_back(outDescriptorSet);
	}

	return true;
}

uint32_t DescriptorAllocator::createPool(uint32_t maxSets, vk::DescriptorPoolCreateFlags flags) {

	std::vector<vk::DescriptorPoolSize> poolSizes;
	
	for (auto it = m_poolSizeFactors.begin(); it != m_poolSizeFactors.end(); ++it) {
		vk::DescriptorType descriptorType = it->first;
		uint32_t descriptorCount = (uint32_t)(glm::max(0.0F, it->second) * maxSets);
		if (descriptorCount <= 0)
			continue; // No pools for this descriptor type, skip it.

		vk::DescriptorPoolSize& poolSize = poolSizes.emplace_back();
		poolSize.setType(descriptorType);
		poolSize.setDescriptorCount(descriptorCount);
	}

	if (m_canFreeDescriptorSets) {
		flags |= vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	}

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.setFlags(flags);
	descriptorPoolCreateInfo.setMaxSets(maxSets);
	descriptorPoolCreateInfo.setPoolSizes(poolSizes);

	try {
		uint32_t index = m_descriptorPools.size();
		m_descriptorPools.push_back(std::make_unique<vkr::DescriptorPool>(*m_device, descriptorPoolCreateInfo));
		return index;
	} catch (std::exception e) {
		printf("Failed to create descriptor pool with %d sets\n", maxSets);
		return UINT32_MAX;
	}
}

uint32_t DescriptorAllocator::grabPool() {
	if (m_freePoolIndices.size() > 0) {
		uint32_t poolIndex = m_freePoolIndices.back();
		m_freePoolIndices.pop_back();
		return poolIndex;

	} else {
		return createPool(1000, {});
	}
}

DescriptorLayoutCache::DescriptorLayoutCache(std::shared_ptr<vkr::Device> device):
	m_device(device) {
}

DescriptorLayoutCache::~DescriptorLayoutCache() {
}

std::shared_ptr<vkr::DescriptorSetLayout> DescriptorLayoutCache::getDescriptorSetLayout(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo) {
	bool sorted = true;
	int prevBinding = -1;

	DescriptorSetLayoutKey key;
	key.flags = (uint32_t)descriptorSetLayoutCreateInfo.flags;

	for (int i = 0; i < descriptorSetLayoutCreateInfo.bindingCount; ++i) {
		const vk::DescriptorSetLayoutBinding& binding = descriptorSetLayoutCreateInfo.pBindings[i];

		if ((int)binding.binding > prevBinding) {
			prevBinding = binding.binding;
		} else {
			sorted = false;
		}

		key.bindings.push_back(binding);
	}

	if (!sorted) {
		std::sort(key.bindings.begin(), key.bindings.end(), [](const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) {
			return lhs.binding < rhs.binding;
		});
	}


	vk::DescriptorSetLayoutCreateInfo createInfo;
	createInfo.setFlags(descriptorSetLayoutCreateInfo.flags);
	createInfo.setBindings(key.bindings);

	auto it = m_layouts.find(key);
	if (it == m_layouts.end()) {
		std::shared_ptr<vkr::DescriptorSetLayout> descriptorSetLayout = std::make_shared<vkr::DescriptorSetLayout>(*m_device, createInfo);
		m_layouts.insert(std::make_pair(key, descriptorSetLayout));
		return descriptorSetLayout;
	}

	return it->second;
}

size_t DescriptorLayoutCache::DescriptorSetLayoutKeyHasher::operator()(const DescriptorSetLayoutKey& descriptorSetLayoutKey) const {
	size_t s = 0;
	std::hash_combine(s, (uint32_t)descriptorSetLayoutKey.flags);
	for (int i = 0; i < descriptorSetLayoutKey.bindings.size(); ++i) {
		std::hash_combine(s, descriptorSetLayoutKey.bindings[i].binding);
		std::hash_combine(s, descriptorSetLayoutKey.bindings[i].descriptorType);
		std::hash_combine(s, descriptorSetLayoutKey.bindings[i].descriptorCount);
		std::hash_combine(s, descriptorSetLayoutKey.bindings[i].stageFlags);
		std::hash_combine(s, descriptorSetLayoutKey.bindings[i].pImmutableSamplers);
	}
	return s;
}


bool DescriptorLayoutCache::DescriptorSetLayoutKey::operator==(const DescriptorSetLayoutKey& rhs) const {
	if (flags != rhs.flags)
		return false;
	if (bindings != rhs.bindings) // vk::DescriptorSetLayoutBinding has a valid operator== implementation
		return false;
	return true;
}

DescriptorBuilder::DescriptorBuilder(DescriptorAllocator* allocator, DescriptorLayoutCache* layoutCache): 
	m_allocator(allocator),
	m_layoutCache(layoutCache) {
}

DescriptorBuilder::~DescriptorBuilder() {

}

DescriptorBuilder& DescriptorBuilder::bindBuffers(uint32_t bindingIndex, const std::vector<vk::DescriptorBufferInfo>& descriptorBufferInfos, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags) {

	// Copy descriptorBufferInfos into local class member to avoid dangling pointers
	uint32_t startTempIndex = m_tempBuffers.size();
	m_tempBuffers.resize(descriptorBufferInfos.size());
	memcpy(&m_tempBuffers[startTempIndex], &descriptorBufferInfos[0], sizeof(vk::DescriptorBufferInfo) * descriptorBufferInfos.size());

	vk::DescriptorSetLayoutBinding& binding = m_bindings.emplace_back();
	binding.setBinding(bindingIndex);
	binding.setDescriptorType(descriptorType);
	binding.setDescriptorCount(descriptorBufferInfos.size());
	binding.setStageFlags(stageFlags);
	binding.setPImmutableSamplers(NULL);

	vk::WriteDescriptorSet& write = m_writes.emplace_back();
	write.setDescriptorCount(descriptorBufferInfos.size());
	write.setPBufferInfo(&m_tempBuffers[startTempIndex]);
	write.setDstBinding(bindingIndex);
	write.setDescriptorType(descriptorType);

	return *this;
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(uint32_t bindingIndex, const vk::DescriptorBufferInfo& descriptorBufferInfo, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags) {
	return bindBuffers(bindingIndex, { descriptorBufferInfo }, descriptorType, stageFlags);
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(uint32_t bindingIndex, vk::Buffer buffer, size_t offset, size_t range, vk::DescriptorType descriptorType, vk::ShaderStageFlags stageFlags) {
	vk::DescriptorBufferInfo descriptorBufferInfo;
	descriptorBufferInfo.setBuffer(buffer);
	descriptorBufferInfo.setOffset(offset);
	descriptorBufferInfo.setRange(range);
	return bindBuffers(bindingIndex, { descriptorBufferInfo }, descriptorType, stageFlags);
}

bool DescriptorBuilder::build(std::shared_ptr<vkr::DescriptorSet>& outDescriptorSet, std::shared_ptr<vkr::DescriptorSetLayout>& outDescriptorSetLayout) {
	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.setBindings(m_bindings);

	std::shared_ptr<vkr::DescriptorSetLayout> descriptorSetLayout = m_layoutCache->getDescriptorSetLayout(descriptorSetLayoutCreateInfo);
	if (descriptorSetLayout == NULL) {
		printf("Unable to create descriptor set. Failed to create descriptor set layout\n");
		reset();
		return false;
	}

	if (!m_allocator->allocate(**descriptorSetLayout, outDescriptorSet)) {
		printf("Failed to allocate descriptor set\n");
		reset();
		return false;
	}

	for (int i = 0; i < m_writes.size(); ++i) {
		m_writes[i].setDstSet(**outDescriptorSet);
	}

	outDescriptorSet->getDevice().updateDescriptorSets(m_writes.size(), m_writes.data(), 0, NULL);

	outDescriptorSetLayout = descriptorSetLayout;
	reset();
	return true;
}

void DescriptorBuilder::reset() {
	m_bindings.clear();
	m_writes.clear();
	m_tempBuffers.clear();
}
