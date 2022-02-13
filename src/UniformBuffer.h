#pragma once

#include "../core.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "GraphicsPipeline.h"

class UniformBuffer {
	NO_COPY(UniformBuffer);

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

		UniformBuffer* build() const;

	private:
		std::shared_ptr<DescriptorPool> m_descriptorPool;
		vk::DeviceSize m_uniformBufferSize;
		SetBindingMap m_setBindings;
	};

private:
	UniformBuffer(
		std::weak_ptr<DescriptorPool> descriptorPool,
		std::weak_ptr<Buffer> uniformBuffer,
		DescriptorSetMap descriptorSets,
		SetBindingMap setBindings);

public:
	~UniformBuffer();

	DescriptorSetWriter writer(uint32_t set);
	
	void startBatchWrite(uint32_t set);

	void endBatchWrite(uint32_t set);

	void writeImage(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo);

	void writeImage(uint32_t set, uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout);

	void writeImage(uint32_t set, uint32_t binding, Sampler* sampler, ImageView2D* imageView, vk::ImageLayout imageLayout);

	void writeImage(uint32_t set, uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout);

	void update(uint32_t set, uint32_t binding, void* data, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	void bind(uint32_t set, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline);

	const vk::DescriptorSetLayout& getDescriptorSetLayout(uint32_t set) const;

private:
	std::shared_ptr<DescriptorPool> m_descriptorPool;
	std::shared_ptr<Buffer> m_uniformBuffer;
	DescriptorSetMap m_descriptorSets;
	SetBindingMap m_setBindings;

	std::unordered_map<uint32_t, DescriptorSetWriter> m_activeWriters;
};



template<class T>
inline UniformBuffer::Builder& UniformBuffer::Builder::addUniformBlock(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages) {
	return addUniformBlock(set, binding, sizeof(T), shaderStages);
}
