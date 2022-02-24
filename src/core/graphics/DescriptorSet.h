#pragma once

#include "../core.h"

class Buffer;
class Sampler;
class ImageView2D;
class Texture2D;

class DescriptorSet;
class DescriptorPool;

struct DescriptorPoolConfiguration {
	std::weak_ptr<vkr::Device> device;
	vk::DescriptorPoolCreateFlags flags;
	uint32_t maxSets = 1000;

	std::unordered_map<vk::DescriptorType, uint32_t> poolSizes = {
		{ vk::DescriptorType::eSampler, 500 },
		{ vk::DescriptorType::eCombinedImageSampler, 4000 },
		{ vk::DescriptorType::eSampledImage, 4000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 2000 },
		{ vk::DescriptorType::eStorageBuffer, 2000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 500 }
	};
};


class DescriptorSetLayout {
	NO_COPY(DescriptorSetLayout);

public:
	struct Key : public vk::DescriptorSetLayoutCreateInfo {
		Key(const vk::DescriptorSetLayoutCreateInfo& rhs);

		Key(const Key& copy);

		Key(Key&& move);

		~Key();

		bool operator==(const Key& rhs) const;
	};

	struct KeyHasher {
		size_t operator()(const Key& descriptorSetLayoutKey) const;
	};

	typedef std::unordered_map<Key, std::weak_ptr<DescriptorSetLayout>, KeyHasher> Cache;

private:
	DescriptorSetLayout(std::weak_ptr<vkr::Device> device, vk::DescriptorSetLayout descriptorSetLayout, Key key);

public:
	~DescriptorSetLayout();

	static std::shared_ptr<DescriptorSetLayout> get(std::weak_ptr<vkr::Device> device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo);

	static void clearCache();

	DescriptorSet* createDescriptorSet(std::shared_ptr<DescriptorPool> descriptorPool);

	bool createDescriptorSets(std::shared_ptr<DescriptorPool> descriptorPool, uint32_t count, DescriptorSet** outDescriptorSets);

	bool createDescriptorSets(std::shared_ptr<DescriptorPool> descriptorPool, uint32_t count, std::shared_ptr<DescriptorSet>* outDescriptorSets);

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

	std::vector<vk::DescriptorSetLayoutBinding> getBindings() const;

	bool hasBinding(uint32_t binding) const;

	int findBindingIndex(uint32_t binding) const;

	const vk::DescriptorSetLayoutBinding& findBinding(uint32_t binding) const;

	const vk::DescriptorSetLayoutBinding& getBinding(int index) const;

	uint32_t getBindingCount() const;

	bool operator==(const DescriptorSetLayout& rhs) const;

	bool operator!=(const DescriptorSetLayout& rhs) const;

private:
	std::shared_ptr<vkr::Device> m_device;
	vk::DescriptorSetLayout m_descriptorSetLayout;
	Key m_key;

	static Cache s_descriptorSetLayoutCache;
};



class DescriptorSetLayoutBuilder {
	NO_COPY(DescriptorSetLayoutBuilder);
public:
	DescriptorSetLayoutBuilder(std::weak_ptr<vkr::Device> device, vk::DescriptorSetLayoutCreateFlags flags = {});

	DescriptorSetLayoutBuilder(vk::DescriptorSetLayoutCreateFlags flags = {});

	~DescriptorSetLayoutBuilder();

	DescriptorSetLayoutBuilder& addUniformBlock(uint32_t binding, vk::ShaderStageFlags shaderStages, size_t sizeBytes);

	DescriptorSetLayoutBuilder& addSampler(uint32_t binding, vk::ShaderStageFlags shaderStages, size_t arraySize = 1);

	DescriptorSetLayoutBuilder& addCombinedImageSampler(uint32_t binding, vk::ShaderStageFlags shaderStages, size_t arraySize = 1);

	std::shared_ptr<DescriptorSetLayout> build();

	DescriptorSetLayoutBuilder& reset(vk::DescriptorSetLayoutCreateFlags flags = {});

private:
	std::weak_ptr<vkr::Device> m_device;
	vk::DescriptorSetLayoutCreateFlags m_flags;
	std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> m_bindings;
};


class DescriptorPool {
	NO_COPY(DescriptorPool);
private:
	DescriptorPool(std::weak_ptr<vkr::Device> device, vk::DescriptorPool descriptorPool, bool canFreeDescriptorSets);

public:
	~DescriptorPool();

	static DescriptorPool* create(const DescriptorPoolConfiguration& descriptorPoolConfiguration);

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::DescriptorPool& getDescriptorPool() const;

	bool allocate(const vk::DescriptorSetLayout& descriptorSetLayout, vk::DescriptorSet& outDescriptorSet) const;

	void free(const vk::DescriptorSet& descriptorSet);

	bool canFreeDescriptorSets() const;

private:
	std::shared_ptr<vkr::Device> m_device;
	vk::DescriptorPool m_descriptorPool;
	bool m_canFreeDescriptorSets;
};



class DescriptorSet {
	NO_COPY(DescriptorSet);
private:
	DescriptorSet(std::weak_ptr<vkr::Device> device, std::weak_ptr<DescriptorPool> pool, std::weak_ptr<DescriptorSetLayout> layout, vk::DescriptorSet descriptorSet);

public:
	~DescriptorSet();

	static DescriptorSet* create(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, std::weak_ptr<DescriptorPool> descriptorPool);

	static DescriptorSet* create(std::weak_ptr<DescriptorSetLayout> descriptorSetLayout, std::weak_ptr<DescriptorPool> descriptorPool);

	const vk::DescriptorSet& getDescriptorSet() const;

	std::shared_ptr<vkr::Device> getDevice() const;

	std::shared_ptr<DescriptorPool> getPool() const;

	std::shared_ptr<DescriptorSetLayout> getLayout() const;

private:
	std::shared_ptr<vkr::Device> m_device;
	std::shared_ptr<DescriptorPool> m_pool;
	std::shared_ptr<DescriptorSetLayout> m_layout;
	vk::DescriptorSet m_descriptorSet;
};


class DescriptorSetWriter {
	NO_COPY(DescriptorSetWriter);
public:
	DescriptorSetWriter(DescriptorSet* descriptorSet);

	DescriptorSetWriter(std::shared_ptr<DescriptorSet> descriptorSet);

	DescriptorSetWriter(std::weak_ptr<DescriptorSet> descriptorSet);

	~DescriptorSetWriter();

	DescriptorSetWriter& writeBuffer(uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo);

	DescriptorSetWriter& writeBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	DescriptorSetWriter& writeBuffer(uint32_t binding, Buffer* buffer, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	DescriptorSetWriter& writeImage(uint32_t binding, const vk::DescriptorImageInfo& imageInfo);

	DescriptorSetWriter& writeImage(uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout);

	DescriptorSetWriter& writeImage(uint32_t binding, Sampler* sampler, ImageView2D* imageView, vk::ImageLayout imageLayout);

	DescriptorSetWriter& writeImage(uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout);

	bool write();

private:
	std::vector<vk::WriteDescriptorSet> m_writes;
	DescriptorSet* m_descriptorSet;

	std::vector<vk::DescriptorBufferInfo> m_tempBufferInfo;
	std::vector<vk::DescriptorImageInfo> m_tempImageInfo;
};