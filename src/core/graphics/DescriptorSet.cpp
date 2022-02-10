#include "DescriptorSet.h"
#include "Buffer.h"

DescriptorSetLayout::Cache DescriptorSetLayout::s_descriptorSetLayoutCache;


DescriptorSetLayout::DescriptorSetLayout(std::shared_ptr<vkr::Device> device, vk::DescriptorSetLayout descriptorSetLayout, Key key):
	m_device(std::move(device)),
	m_descriptorSetLayout(descriptorSetLayout), 
	m_key(key) {
}

DescriptorSetLayout::~DescriptorSetLayout() {
	(**m_device).destroyDescriptorSetLayout(m_descriptorSetLayout);
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayout::get(std::shared_ptr<vkr::Device> device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo) {
	Key key(descriptorSetLayoutCreateInfo);

#if _DEBUG
	// Check for duplicated bindings
	int lastBinding = -1;
	for (int i = 0; i < key.bindingCount; ++i) {
		int binding = (int)key.pBindings[i].binding;
		if (binding == lastBinding) {
			printf("Descriptor set layout has duplicated bindings\n");
			assert(false);
			return NULL;
		}
	}
#endif

	auto it = s_descriptorSetLayoutCache.find(key);
	if (it == s_descriptorSetLayoutCache.end()) {

		vk::DescriptorSetLayout descriptorSetLayout = (**device).createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
		std::shared_ptr<DescriptorSetLayout> retDescriptorSetLayout(new DescriptorSetLayout(device, descriptorSetLayout, key));
		s_descriptorSetLayoutCache.insert(std::make_pair(key, retDescriptorSetLayout));
		return retDescriptorSetLayout;
	}

	return it->second;
}

void DescriptorSetLayout::clearCache() {
#if _DEBUG
	int count = 0;

	for (auto it = s_descriptorSetLayoutCache.begin(); it != s_descriptorSetLayoutCache.end(); ++it) {
		auto& layoutPtr = it->second;
		if (layoutPtr.use_count() > 1) {
			++count;
		}
	}

	if (count > 0) {
		printf("Clearing descriptor set layout cache. %d descriptor set layouts have external references and will not be freed\n", count);
	}
#endif

	s_descriptorSetLayoutCache.clear();
}

std::shared_ptr<vkr::Device> DescriptorSetLayout::getDevice() const {
	return m_device;
}

const vk::DescriptorSetLayout& DescriptorSetLayout::getDescriptorSetLayout() const {
	return m_descriptorSetLayout;
}

std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayout::getBindings() const {
	return std::vector<vk::DescriptorSetLayoutBinding>(m_key.pBindings, m_key.pBindings + m_key.bindingCount);
}

bool DescriptorSetLayout::hasBinding(uint32_t binding) const {
	return findBindingIndex(binding) >= 0;
}

int DescriptorSetLayout::findBindingIndex(uint32_t binding) const {
	for (int i = 0; i < m_key.bindingCount; ++i) {
		if (m_key.pBindings[i].binding == binding) {
			return i;
		}
	}

	return -1;
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::findBinding(uint32_t binding) const {
	int index = findBindingIndex(binding);
	if (index < 0)
		return {};
	return getBinding(index);
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::getBinding(int index) const {
	assert(index >= 0 && index < m_key.bindingCount);
	return m_key.pBindings[index];
}

uint32_t DescriptorSetLayout::getBindingCount() const {
	return m_key.bindingCount;
}

bool DescriptorSetLayout::operator==(const DescriptorSetLayout& rhs) const {
	return m_key == rhs.m_key;
}

bool DescriptorSetLayout::operator!=(const DescriptorSetLayout& rhs) const {
	return !(*this == rhs);
}






DescriptorPool::DescriptorPool(std::shared_ptr<vkr::Device> device, vk::DescriptorPool descriptorPool, bool canFreeDescriptorSets):
	m_device(std::move(device)),
	m_descriptorPool(descriptorPool), 
	m_canFreeDescriptorSets(canFreeDescriptorSets) {
}

DescriptorPool::~DescriptorPool() {
	if (m_canFreeDescriptorSets) {
		// TODO: should we keep a reference to all DescriptorSetLayouts allocated via this pool and free them here??
	} else {
		(**m_device).resetDescriptorPool(m_descriptorPool);
	}

	(**m_device).destroyDescriptorPool(m_descriptorPool);
}

std::shared_ptr<DescriptorPool> DescriptorPool::create(const DescriptorPoolConfiguration& descriptorPoolConfiguration) {
	std::vector<vk::DescriptorPoolSize> poolSizes;

	for (auto it = descriptorPoolConfiguration.poolSizes.begin(); it != descriptorPoolConfiguration.poolSizes.end(); ++it) {
		if (it->second == 0)
			continue; // Skip zero-size pools

		vk::DescriptorPoolSize& poolSize = poolSizes.emplace_back();
		poolSize.setType(it->first);
		poolSize.setDescriptorCount(it->second);
	}

	vk::DescriptorPoolCreateInfo createInfo;
	createInfo.setFlags(descriptorPoolConfiguration.flags);
	createInfo.setMaxSets(descriptorPoolConfiguration.maxSets);
	createInfo.setPoolSizes(poolSizes);

	bool canFreeDescriptorSets = (bool)(createInfo.flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

	vk::DescriptorPool descriptorPool = (**descriptorPoolConfiguration.device).createDescriptorPool(createInfo);
	return std::shared_ptr<DescriptorPool>(new DescriptorPool(descriptorPoolConfiguration.device, descriptorPool, canFreeDescriptorSets));
	
}

std::shared_ptr<vkr::Device> DescriptorPool::getDevice() const {
	return m_device;
}

const vk::DescriptorPool& DescriptorPool::getDescriptorPool() const {
	return m_descriptorPool;
}

bool DescriptorPool::allocate(const vk::DescriptorSetLayout& descriptorSetLayout, vk::DescriptorSet& outDescriptorSet) const {

	vk::DescriptorSetAllocateInfo allocateInfo;
	allocateInfo.setDescriptorPool(m_descriptorPool);
	allocateInfo.setDescriptorSetCount(1);
	allocateInfo.setPSetLayouts(&descriptorSetLayout);
	vk::Result result = (**m_device).allocateDescriptorSets(&allocateInfo, &outDescriptorSet);

	return result == vk::Result::eSuccess;
}

void DescriptorPool::free(const vk::DescriptorSet& descriptorSet) {
	(**m_device).freeDescriptorSets(m_descriptorPool, 1, &descriptorSet);
}

bool DescriptorPool::canFreeDescriptorSets() const {
	return m_canFreeDescriptorSets;
}



DescriptorSet::DescriptorSet(std::shared_ptr<vkr::Device> device, std::shared_ptr<DescriptorPool> pool, std::shared_ptr<DescriptorSetLayout> layout, vk::DescriptorSet descriptorSet):
	m_device(std::move(device)),
	m_pool(std::move(pool)),
	m_layout(std::move(layout)),
	m_descriptorSet(descriptorSet) {
}

DescriptorSet::~DescriptorSet() {
	if (m_pool->canFreeDescriptorSets()) {
		(**m_device).freeDescriptorSets(m_pool->getDescriptorPool(), 1, &m_descriptorSet);
	}
}

std::shared_ptr<DescriptorSet> DescriptorSet::get(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, std::shared_ptr<DescriptorPool> descriptorPool) {
	assert(descriptorPool != NULL);
	std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayout::get(descriptorPool->getDevice(), descriptorSetLayoutCreateInfo);
	return DescriptorSet::get(descriptorSetLayout, descriptorPool);
}

std::shared_ptr<DescriptorSet> DescriptorSet::get(std::shared_ptr<DescriptorSetLayout> descriptorSetLayout, std::shared_ptr<DescriptorPool> descriptorPool) {
	assert(descriptorSetLayout != NULL && descriptorPool != NULL);
	assert(descriptorSetLayout->getDevice() == descriptorPool->getDevice());

	const std::shared_ptr<vkr::Device>& device = descriptorPool->getDevice();

	vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;
	bool allocated = descriptorPool->allocate(descriptorSetLayout->getDescriptorSetLayout(), descriptorSet);
	assert(allocated && descriptorSet);

	return std::shared_ptr<DescriptorSet>(new DescriptorSet(device, descriptorPool, descriptorSetLayout, descriptorSet));
}

const vk::DescriptorSet& DescriptorSet::getDescriptorSet() const {
	return m_descriptorSet;
}

std::shared_ptr<vkr::Device> DescriptorSet::getDevice() const {
	return m_device;
}

std::shared_ptr<DescriptorPool> DescriptorSet::getPool() const {
	return m_pool;
}

std::shared_ptr<DescriptorSetLayout> DescriptorSet::getLayout() const {
	return m_layout;
}



DescriptorSetWriter::DescriptorSetWriter(std::shared_ptr<DescriptorSet> descriptorSet):
	m_descriptorSet(std::move(descriptorSet)) {
}

DescriptorSetWriter::~DescriptorSetWriter() {

}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo) {
	int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
	assert(bindingIndex >= 0);

	const auto& bindingInfo = m_descriptorSet->getLayout()->getBinding(bindingIndex);

	assert(bindingInfo.descriptorCount == 1);

	m_tempBufferInfo.emplace_back(bufferInfo);

	vk::WriteDescriptorSet& write = m_writes.emplace_back();
	write.setDstSet(m_descriptorSet->getDescriptorSet());
	write.setDescriptorType(bindingInfo.descriptorType);
	write.setDstBinding(binding);
	write.setPBufferInfo(&m_tempBufferInfo.back());
	write.setDescriptorCount(1);

	return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize range) {
	vk::DescriptorBufferInfo bufferInfo;
	bufferInfo.setBuffer(buffer);
	bufferInfo.setOffset(offset);
	bufferInfo.setRange(range);
	return writeBuffer(binding, bufferInfo);
}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, Buffer* buffer, vk::DeviceSize offset, vk::DeviceSize range) {
	vk::DescriptorBufferInfo bufferInfo;
	bufferInfo.setBuffer(buffer->getBuffer());
	bufferInfo.setOffset(offset);
	bufferInfo.setRange(range);
	return writeBuffer(binding, bufferInfo);
}

bool DescriptorSetWriter::write() {
	const auto& device = m_descriptorSet->getDevice();
	device->updateDescriptorSets(m_writes, {});
	return true;
}



DescriptorSetLayout::Key::Key(const vk::DescriptorSetLayoutCreateInfo& rhs) :
	vk::DescriptorSetLayoutCreateInfo(rhs) {

	// Ensure bindings array is sorted
	int prevBinding = -1;
	for (int i = 0; i < bindingCount; ++i) {
		const vk::DescriptorSetLayoutBinding& binding = pBindings[i];

		if ((int)binding.binding > prevBinding) {
			prevBinding = binding.binding;
		} else {
			// bindings array is not sorted

			vk::DescriptorSetLayoutBinding* bindingsArr = const_cast<vk::DescriptorSetLayoutBinding*>(pBindings);
			std::sort(bindingsArr, bindingsArr + bindingCount, [](vk::DescriptorSetLayoutBinding lhs, vk::DescriptorSetLayoutBinding rhs) {
				return lhs.binding < rhs.binding;
			});

			break;
		}
	}
}

bool DescriptorSetLayout::Key::operator==(const DescriptorSetLayout::Key& rhs) const {
	if (flags != rhs.flags)
		return false;
	if (bindingCount != rhs.bindingCount)
		return false;
	for (int i = 0; i < bindingCount; ++i)
		if (pBindings[i] != rhs.pBindings[i]) // vk::DescriptorSetLayoutBinding has a valid operator== definition
			return false;

	return true;
}

size_t DescriptorSetLayout::KeyHasher::operator()(const DescriptorSetLayout::Key& descriptorSetLayoutKey) const {
	size_t s = 0;
	std::hash_combine(s, (uint32_t)descriptorSetLayoutKey.flags);
	for (int i = 0; i < descriptorSetLayoutKey.bindingCount; ++i) {
		std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].binding);
		std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].descriptorType);
		std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].descriptorCount);
		std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].stageFlags);
		std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].pImmutableSamplers);
	}
	return s;
}