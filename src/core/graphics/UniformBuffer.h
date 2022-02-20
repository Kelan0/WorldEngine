#pragma once

#include "../core.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "GraphicsPipeline.h"

class ShaderResources {
	NO_COPY(ShaderResources);

private:
	struct Binding {
		uint32_t binding;
		vk::DescriptorType descriptorType;
		uint32_t descriptorCount;
		vk::ShaderStageFlags shaderStages;
		vk::DeviceSize bufferOffset;
		vk::DeviceSize bufferRange;
		vk::Sampler sampler = VK_NULL_HANDLE;
		vk::ImageView imageView = VK_NULL_HANDLE;
		vk::ImageLayout imageLayout;
	};

	typedef std::unordered_map<uint32_t, Binding> BindingMap;
	typedef std::unordered_map<uint32_t, BindingMap> SetBindingMap;
	typedef std::unordered_map<uint32_t, std::shared_ptr<DescriptorSet>>  DescriptorSetMap;

public:
	class Builder {
	public:
		Builder(std::weak_ptr<DescriptorPool> descriptorPool);

		~Builder();

		Builder& addUniformBlock(uint32_t set, uint32_t binding, vk::DeviceSize dataSize, vk::ShaderStageFlags shaderStages);

		template<class T>
		Builder& addUniformBlock(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages);

		Builder& addTextureSampler(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages);

		ShaderResources* build() const;

	private:
		std::shared_ptr<DescriptorPool> m_descriptorPool;
		vk::DeviceSize m_uniformBufferSize;
		SetBindingMap m_setBindings;
	};

private:
	ShaderResources(
		std::weak_ptr<DescriptorPool> descriptorPool,
		std::weak_ptr<Buffer> uniformBuffer,
		DescriptorSetMap descriptorSets,
		SetBindingMap setBindings);

public:
	~ShaderResources();

	DescriptorSetWriter writer(uint32_t set);

	void startBatchWrite(uint32_t set);

	void endBatchWrite(uint32_t set);

	void writeBuffer(uint32_t set, uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo);

	void writeBuffer(uint32_t set, uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	void writeBuffer(uint32_t set, uint32_t binding, Buffer* buffer, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	void writeImage(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo);

	void writeImage(uint32_t set, uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout);

	void writeImage(uint32_t set, uint32_t binding, Sampler* sampler, ImageView2D* imageView, vk::ImageLayout imageLayout);

	void writeImage(uint32_t set, uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout);

	void update(uint32_t set, uint32_t binding, void* data, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	template<class T>
	void update(uint32_t set, uint32_t binding, T* data, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	void bind(uint32_t set, uint32_t shaderSet, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline);

	void bind(std::vector<uint32_t> sets, uint32_t firstShaderSet, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline);

	const vk::DescriptorSetLayout& getDescriptorSetLayout(uint32_t set) const;

	void initPipelineConfiguration(GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

private:
	std::shared_ptr<DescriptorPool> m_descriptorPool;
	std::shared_ptr<Buffer> m_uniformBuffer;
	DescriptorSetMap m_descriptorSets;
	SetBindingMap m_setBindings;

	std::unordered_map<uint32_t, DescriptorSetWriter*> m_activeWriters;
};



template<class T>
inline ShaderResources::Builder& ShaderResources::Builder::addUniformBlock(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages) {
	return addUniformBlock(set, binding, sizeof(T), shaderStages);
}

template<class T>
inline void ShaderResources::update(uint32_t set, uint32_t binding, T* data, vk::DeviceSize offset, vk::DeviceSize range) {
	update(set, binding, (void*)(data), offset, range);
}
