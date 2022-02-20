#include "UniformBuffer.h"


ShaderResources::Builder::Builder(std::weak_ptr<DescriptorPool> descriptorPool) :
	m_descriptorPool(descriptorPool),
	m_uniformBufferSize(0) {
}

ShaderResources::Builder::~Builder() {
}

ShaderResources::Builder& ShaderResources::Builder::addUniformBlock(uint32_t set, uint32_t binding, vk::DeviceSize dataSize, vk::ShaderStageFlags shaderStages) {
	BindingMap& bindings = m_setBindings[set];

#if _DEBUG
	auto it = bindings.find(binding);
	if (it != bindings.end()) {
		printf("Unable to add uniform block (setOrtho = %d, binding = %d): The binding is already used\n", set, binding);
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

ShaderResources::Builder& ShaderResources::Builder::addTextureSampler(uint32_t set, uint32_t binding, vk::ShaderStageFlags shaderStages) {
	BindingMap& bindings = m_setBindings[set];

#if _DEBUG
	auto it = bindings.find(binding);
	if (it != bindings.end()) {
		printf("Unable to add texture sampler (setOrtho = %d, binding = %d): The binding is already used\n", set, binding);
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

ShaderResources* ShaderResources::Builder::build() const {

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
				printf("Unable to write descriptor setOrtho binding %d: The descriptor type %s is not implemented\n", i, vk::to_string(bindingInfo.descriptorType).c_str());
				break;
			}
		}

		writer.write();

		descriptorSets.insert(std::make_pair(setIndex, descriptorSet));
	}

	return new ShaderResources(m_descriptorPool, std::move(uniformBuffer), descriptorSets, m_setBindings);
}



ShaderResources::ShaderResources(
	std::weak_ptr<DescriptorPool> descriptorPool,
	std::weak_ptr<Buffer> uniformBuffer,
	DescriptorSetMap descriptorSets,
	SetBindingMap setBindings) :
	m_descriptorPool(descriptorPool),
	m_uniformBuffer(uniformBuffer),
	m_descriptorSets(descriptorSets),
	m_setBindings(setBindings) {
}

ShaderResources::~ShaderResources() {
	m_descriptorPool.reset();
	m_descriptorSets.clear();
	m_setBindings.clear();
	m_uniformBuffer.reset();
}

void ShaderResources::update(uint32_t set, uint32_t binding, void* data, vk::DeviceSize offset, vk::DeviceSize range) {
	const Binding& bindingInfo = m_setBindings.at(set).at(binding);

#if _DEBUG
	if (m_activeWriters.count(set) > 0) {
		printf("Unable to update buffer [setOrtho = %d, binding = %d] for this uniform buffer: batch write for setOrtho index %d was not ended\n", set);
		assert(false);
		return;
	}
	if (offset >= bindingInfo.bufferRange) {
		printf("Unable to update buffer [setOrtho = %d, binding = %d] for this uniform buffer: buffer offset out of range\n", set, binding);
		assert(false);
		return;
	}
#endif

	if (range == VK_WHOLE_SIZE)
		range = bindingInfo.bufferRange - offset;

#if _DEBUG
	if (offset + range > bindingInfo.bufferRange) {
		printf("Unable to update buffer [setOrtho = %d, binding = %d] for this uniform buffer: buffer range out of range\n", set, binding);
		assert(false);
		return;
	}
#endif

	m_uniformBuffer->upload(bindingInfo.bufferOffset + offset, range, data);
}

void ShaderResources::bind(uint32_t set, uint32_t shaderSet, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline) {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to bind descriptor setOrtho: setOrtho index %d does not exist\n", set);
		assert(false);
		return;
	}
	if (m_activeWriters.count(set) > 0) {
		printf("Unable to bind descriptor setOrtho: batch write for setOrtho index %d was not ended\n", set);
		assert(false);
		return;
	}
#endif
	const auto& descriptorSet = m_descriptorSets.at(set);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getPipelineLayout(), shaderSet, 1, &descriptorSet->getDescriptorSet(), 0, NULL);
}

void ShaderResources::bind(std::vector<uint32_t> sets, uint32_t firstShaderSet, const vk::CommandBuffer& commandBuffer, const GraphicsPipeline& graphicsPipeline) {

#if _DEBUG
	for (int i = 0; i < sets.size(); ++i) {
		if (m_descriptorSets.count(sets[i]) == 0) {
			printf("Unable to bind descriptor sets: setOrtho index %d does not exist\n", sets[i]);
			assert(false);
			return;
		}
		if (m_activeWriters.count(sets[i]) > 0) {
			printf("Unable to bind descriptor sets: batch write for setOrtho index %d was not ended\n", sets[i]);
			assert(false);
			return;
		}
	}
#endif

	std::vector<vk::DescriptorSet> descriptorSets;
	descriptorSets.resize(sets.size());

	for (int i = 0; i < sets.size(); ++i) {
		descriptorSets[i] = m_descriptorSets.at(sets[i])->getDescriptorSet();
	}

	std::vector<uint32_t> dynamicOffsets;

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getPipelineLayout(), firstShaderSet, descriptorSets, dynamicOffsets);
}

DescriptorSetWriter ShaderResources::writer(uint32_t set) {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to create descriptor setOrtho writer: setOrtho index %d does not exist\n", set);
		assert(false);
	}
#endif
	return DescriptorSetWriter(m_descriptorSets.at(set));
}

void ShaderResources::writeImage(uint32_t set, uint32_t binding, const vk::DescriptorImageInfo& imageInfo) {
	writer(set).writeImage(binding, imageInfo).write();
}

void ShaderResources::startBatchWrite(uint32_t set) {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to create descriptor setOrtho writer: setOrtho index %d does not exist\n", set);
		assert(false);
		return;
	}
	if (m_activeWriters.count(set) != 0) {
		printf("Batch write for setOrtho %d of this uniform buffer was already started\n", set);
		assert(false);
		return;
	}
#endif

	m_activeWriters.insert(std::make_pair(set, new DescriptorSetWriter(m_descriptorSets.at(set))));
}

void ShaderResources::endBatchWrite(uint32_t set) {
	auto it = m_activeWriters.find(set);
#if _DEBUG
	if (it == m_activeWriters.end()) {
		printf("Unable to end batch write for setOrtho %d of this uniform buffer. It was never started\n", set);
		assert(false);
		return;
	}
#endif

	it->second->write();
	delete it->second;
	m_activeWriters.erase(it);
}

void ShaderResources::writeBuffer(uint32_t set, uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeBuffer(binding, bufferInfo).write();
	} else {
		it->second->writeBuffer(binding, bufferInfo);
	}
}

void ShaderResources::writeBuffer(uint32_t set, uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeBuffer(binding, buffer, offset, range).write();
	} else {
		it->second->writeBuffer(binding, buffer, offset, range);
	}
}

void ShaderResources::writeBuffer(uint32_t set, uint32_t binding, Buffer* buffer, vk::DeviceSize offset, vk::DeviceSize range) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeBuffer(binding, buffer, offset, range).write();
	} else {
		it->second->writeBuffer(binding, buffer, offset, range);
	}
}

void ShaderResources::writeImage(uint32_t set, uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeImage(binding, sampler, imageView, imageLayout).write();
	} else {
		it->second->writeImage(binding, sampler, imageView, imageLayout);
	}
}

void ShaderResources::writeImage(uint32_t set, uint32_t binding, Sampler* sampler, ImageView2D* imageView, vk::ImageLayout imageLayout) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeImage(binding, sampler, imageView, imageLayout).write();
	} else {
		it->second->writeImage(binding, sampler, imageView, imageLayout);
	}
}

void ShaderResources::writeImage(uint32_t set, uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout) {
	auto it = m_activeWriters.find(set);
	if (it == m_activeWriters.end()) {
		writer(set).writeImage(binding, texture, imageLayout).write();
	} else {
		it->second->writeImage(binding, texture, imageLayout);
	}
}


const vk::DescriptorSetLayout& ShaderResources::getDescriptorSetLayout(uint32_t set) const {
#if _DEBUG
	if (m_descriptorSets.count(set) == 0) {
		printf("Unable to get descriptor setOrtho layout: setOrtho index %d does not exist\n", set);
		assert(false);
	}
#endif
	const auto& descriptorSet = m_descriptorSets.at(set);
	return descriptorSet->getLayout()->getDescriptorSetLayout();
}

void ShaderResources::initPipelineConfiguration(GraphicsPipelineConfiguration& graphicsPipelineConfiguration) {
	for (auto it = m_descriptorSets.begin(); it != m_descriptorSets.end(); ++it) {
		graphicsPipelineConfiguration.descriptorSetLayous.push_back(it->second->getLayout()->getDescriptorSetLayout());
	}
}
