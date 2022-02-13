#include "UniformBuffer.h"


UniformBuffer::Builder::Builder(std::weak_ptr<DescriptorPool> descriptorPool):
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

UniformBuffer::Builder& UniformBuffer::Builder::addTextureSampler(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages) {
	BindingMap& bindings = m_setBindings[set];

#if _DEBUG
	auto it = bindings.find(binding);
	if (it != bindings.end()) {
		printf("Unable to add texture sampler (set = %d, binding = %d): The binding is already used\n", set, binding);
		assert(false);
		return *this;
	}
#endif
	Binding bindingInfo;
	bindingInfo.binding = binding;
	bindingInfo.shaderStages = shaderStages;
	bindingInfo.descriptorCount = 1;
	bindingInfo.descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindings.insert(std::make_pair(binding, bindingInfo));
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
			binding.setPImmutableSamplers(NULL);
		}

		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.setBindings(bindings);
		std::shared_ptr<DescriptorSet> descriptorSet = DescriptorSet::get(layoutCreateInfo, m_descriptorPool);

		DescriptorSetWriter writer(descriptorSet);
		for (int i = 0; i < descriptorSet->getLayout()->getBindingCount(); ++i) {
			uint32_t binding = descriptorSet->getLayout()->getBinding(i).binding;
			const Binding& bindingInfo = setBindings.at(binding);
			switch (bindingInfo.descriptorType) {
			case vk::DescriptorType::eUniformBuffer:
				writer.writeBuffer(bindingInfo.binding, uniformBuffer.get(), bindingInfo.bufferOffset, bindingInfo.bufferRange);
				break;
			case vk::DescriptorType::eCombinedImageSampler:
				if (bindingInfo.sampler && bindingInfo.imageView)
					writer.writeImage(bindingInfo.binding, bindingInfo.sampler, bindingInfo.imageView, bindingInfo.imageLayout);
				break;
			default:
				printf("Unable to write descriptor set binding %d: The descriptor type %s is not implemented\n", i, vk::to_string(bindingInfo.descriptorType).c_str());
				break;
			}
		}

		writer.write();

		descriptorSets.insert(std::make_pair(setIndex, descriptorSet));
	}

	return new UniformBuffer(m_descriptorPool, std::move(uniformBuffer), descriptorSets, m_setBindings);
}



UniformBuffer::UniformBuffer(
	std::weak_ptr<DescriptorPool> descriptorPool,
	std::weak_ptr<Buffer> uniformBuffer,
	DescriptorSetMap descriptorSets,
	SetBindingMap setBindings):
	m_descriptorPool(descriptorPool),
	m_uniformBuffer(uniformBuffer),
	m_descriptorSets(descriptorSets),
	m_setBindings(setBindings) {
}

UniformBuffer::~UniformBuffer() {
	m_descriptorPool.reset();
	m_descriptorSets.clear();
	m_setBindings.clear();
	m_uniformBuffer.reset();
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
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to bind descriptor sets: set index %d does not exist\n", set);
		assert(false);
	}
#endif
	const auto& descriptorSet = m_descriptorSets.at(set);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getPipelineLayout(), set, 1, &descriptorSet->getDescriptorSet(), 0, NULL);
}

DescriptorSetWriter UniformBuffer::writer(uint32_t set) {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to create descriptor set writer: set index %d does not exist\n", set);
		assert(false);
	}
#endif
	return DescriptorSetWriter(m_descriptorSets.at(set));
}

void UniformBuffer::writeImage(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo) {
	writer(set).writeImage(binding, imageInfo).write();
}

void UniformBuffer::startBatchWrite(uint32_t set) {
#if _DEBUG
	if (m_activeWriters.count(set) != 0) {
		printf("Batch write for set %d of this uniform buffer was already started\n", set);
		assert(false);
		return;
	}
#endif

	m_activeWriters.insert(std::make_pair<uint32_t, DescriptorSetWriter>(set, writer(set)));
}

void UniformBuffer::endBatchWrite(uint32_t set) {
}

void UniformBuffer::writeImage(uint32_t set, uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout) {
	writer(set).writeImage(binding, sampler, imageView, imageLayout).write();
}

void UniformBuffer::writeImage(uint32_t set, uint32_t binding, Sampler* sampler, ImageView2D* imageView, vk::ImageLayout imageLayout) {
	writer(set).writeImage(binding, sampler, imageView, imageLayout).write();
}

void UniformBuffer::writeImage(uint32_t set, uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout) {
	writer(set).writeImage(binding, texture, imageLayout).write();
}


const vk::DescriptorSetLayout& UniformBuffer::getDescriptorSetLayout(uint32_t set) const {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to get descriptor set layout: set index %d does not exist\n", set);
		assert(false);
	}
#endif
	const auto& descriptorSet = m_descriptorSets.at(set);
	return descriptorSet->getLayout()->getDescriptorSetLayout();
}