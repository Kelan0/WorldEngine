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
	};

	typedef std::unordered_map<uint32_t, Binding> BindingMap;
	typedef std::unordered_map<uint32_t, BindingMap> SetBindingMap;
	typedef std::unordered_map<uint32_t, std::shared_ptr<DescriptorSet>>  DescriptorSetMap;

public:
	class Builder {
	public:
		Builder(std::shared_ptr<DescriptorPool> descriptorPool);

		~Builder();

		Builder& addUniformBlock(uint32_t set, uint32_t binding, vk::DeviceSize dataSize, vk::ShaderStageFlags shaderStages);

		template<class T> 
		Builder& addUniformBlock(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages);

		UniformBuffer* build() const;

	private:
		std::shared_ptr<DescriptorPool> m_descriptorPool;
		vk::DeviceSize m_uniformBufferSize;
		SetBindingMap m_setBindings;
	};

private:
	UniformBuffer(
		std::shared_ptr<DescriptorPool> descriptorPool, 
		std::shared_ptr<Buffer> uniformBuffer, 
		DescriptorSetMap descriptorSets,
		SetBindingMap setBindings);

public:
	~UniformBuffer();

	void update(uint32_t set, uint32_t binding, void* data, vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE);

	void bind(uint32_t set, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline);

	const vk::DescriptorSetLayout& getDescriptorSetLayout(uint32_t set) const;

private:
	std::shared_ptr<DescriptorPool> m_descriptorPool;
	std::shared_ptr<Buffer> m_uniformBuffer;
	DescriptorSetMap m_descriptorSets;
	SetBindingMap m_setBindings;
};



template<class T>
inline UniformBuffer::Builder& UniformBuffer::Builder::addUniformBlock(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages) {
	return addUniformBlock(set, binding, sizeof(T), shaderStages);
}
