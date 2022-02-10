#include "UniformBuffer.h"


UniformBuffer::Builder::Builder(std::shared_ptr<DescriptorPool> descriptorPool):
	m_descriptorPool(descriptorPool),
	m_uniformBufferSize(0) {
}

UniformBuffer::Builder::~Builder() {
}

UniformBuffer::Builder& UniformBuffer::Builder::addUniformBlock(uint32_t set, uint32_t binding, vk::DeviceSize dataSize, vk::ShaderStageFlags shaderStages) {
	BindingMap& bindings = m_setBindings[set];

#if _DEBUG
	auto it = bindings.find(binding);
	if (it != bindings.end()) {
		printf("Unable to add uniform block (set = %d, binding = %d): The binding is already used\n", set, binding);
		assert(false);
		return *this;
	}
#endif
	Binding bindingInfo;
	bindingInfo.binding = binding;
	bindingInfo.shaderStages = shaderStages;
	bindingInfo.descriptorCount = 1;
	bindingInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
	bindingInfo.bufferOffset = m_uniformBufferSize;
	bindingInfo.bufferRange = dataSize;
	bindings.insert(std::make_pair(binding, bindingInfo));
	m_uniformBufferSize += dataSize;
	return *this;
}

UniformBuffer* UniformBuffer::Builder::build() const {

	BufferConfiguration uniformBufferConfiguration;
	uniformBufferConfiguration.device = m_descriptorPool->getDevice();
	uniformBufferConfiguration.size = m_uniformBufferSize;
	uniformBufferConfiguration.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	uniformBufferConfiguration.usage = vk::BufferUsageFlagBits::eUniformBuffer;
	std::shared_ptr<Buffer> uniformBuffer(Buffer::create(uniformBufferConfiguration));

	DescriptorSetMap descriptorSets;
	for (auto it = m_setBindings.begin(); it != m_setBindings.end(); ++it) {
		uint32_t setIndex = it->first;
		auto& setBindings = it->second;

		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		for (auto it1 = setBindings.begin(); it1 != setBindings.end(); ++it1) {
			const Binding& bindingInfo = it1->second;
			vk::DescriptorSetLayoutBinding& binding = bindings.emplace_back();
			binding.setBinding(bindingInfo.binding);
			binding.setDescriptorType(bindingInfo.descriptorType);
			binding.setDescriptorCount(bindingInfo.descriptorCount);
			binding.setStageFlags(bindingInfo.shaderStages);
		}

		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.setBindings(bindings);
		std::shared_ptr<DescriptorSet> descriptorSet = DescriptorSet::get(layoutCreateInfo, m_descriptorPool);

		DescriptorSetWriter writer(descriptorSet);
		for (int i = 0; i < descriptorSet->getLayout()->getBindingCount(); ++i) {
			uint32_t binding = descriptorSet->getLayout()->getBinding(i).binding;
			const Binding& bindingInfo = setBindings.at(binding);
			writer.writeBuffer(bindingInfo.binding, uniformBuffer.get(), bindingInfo.bufferOffset, bindingInfo.bufferRange);
		}

		writer.write();

		descriptorSets.insert(std::make_pair(setIndex, descriptorSet));
	}

	return new UniformBuffer(m_descriptorPool, std::move(uniformBuffer), descriptorSets, m_setBindings);
}



UniformBuffer::UniformBuffer(
	std::shared_ptr<DescriptorPool> descriptorPool, 
	std::shared_ptr<Buffer> uniformBuffer, 
	DescriptorSetMap descriptorSets,
	SetBindingMap setBindings):
	m_descriptorPool(std::move(descriptorPool)),
	m_uniformBuffer(std::move(uniformBuffer)),
	m_descriptorSets(descriptorSets),
	m_setBindings(setBindings) {
}

UniformBuffer::~UniformBuffer() {
}

void UniformBuffer::update(uint32_t set, uint32_t binding, void* data, vk::DeviceSize offset, vk::DeviceSize range) {
	const Binding& bindingInfo = m_setBindings.at(set).at(binding);
	
	assert(offset < bindingInfo.bufferRange);

	if (range == VK_WHOLE_SIZE)
		range = bindingInfo.bufferRange - offset;
	
	assert(offset + range <= bindingInfo.bufferRange);

	m_uniformBuffer->upload(bindingInfo.bufferOffset + offset, range, data);
}

void UniformBuffer::bind(uint32_t set, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline) {
	const auto& descriptorSet = m_descriptorSets.at(set);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getPipelineLayout(), set, 1, &descriptorSet->getDescriptorSet(), 0, NULL);
}

const vk::DescriptorSetLayout& UniformBuffer::getDescriptorSetLayout(uint32_t set) const {
	const auto& descriptorSet = m_descriptorSets.at(set);
	return descriptorSet->getLayout()->getDescriptorSetLayout();
}
