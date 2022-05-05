#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/BufferView.h"
#include "core/graphics/Texture.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"

DescriptorSetLayout::Cache DescriptorSetLayout::s_descriptorSetLayoutCache;


DescriptorSetLayout::DescriptorSetLayout(std::weak_ptr<vkr::Device> device, vk::DescriptorSetLayout descriptorSetLayout, Key key):
        m_device(device),
        m_descriptorSetLayout(descriptorSetLayout),
        m_key(key),
        m_resourceId(GraphicsManager::nextResourceId()) {
    //printf("Create DescriptorSetLayout\n");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    //printf("Destroy DescriptorSetLayout\n");
    (**m_device).destroyDescriptorSetLayout(m_descriptorSetLayout);
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayout::get(std::weak_ptr<vkr::Device> device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo) {
    Key key(descriptorSetLayoutCreateInfo);

#if _DEBUG
    // Check for duplicated bindings
	int lastBinding = -1;
	for (uint32_t i = 0; i < key.bindingCount; ++i) {
		int binding = (int)key.pBindings[i].binding;
		if (binding == lastBinding) {
			printf("Descriptor set layout has duplicated bindings\n");
			assert(false);
			return NULL;
		}
	}
#endif

    auto it = s_descriptorSetLayoutCache.find(key);
    if (it == s_descriptorSetLayoutCache.end() || it->second.expired()) {
        vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        vk::Result result = (**device.lock()).createDescriptorSetLayout(&descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayout);
        if (result != vk::Result::eSuccess) {
            printf("Failed to create descriptor set layout: %s\n", vk::to_string(result).c_str());
            return NULL;
        }

        std::shared_ptr<DescriptorSetLayout> retDescriptorSetLayout(new DescriptorSetLayout(device, descriptorSetLayout, key));
        s_descriptorSetLayoutCache.insert(std::make_pair(key, std::weak_ptr<DescriptorSetLayout>(retDescriptorSetLayout)));
        return retDescriptorSetLayout;

    } else {
        return std::shared_ptr<DescriptorSetLayout>(it->second);
    }
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

DescriptorSet* DescriptorSetLayout::createDescriptorSet(std::shared_ptr<DescriptorPool> descriptorPool) {
    DescriptorSet* descriptorSet = NULL;
    if (!createDescriptorSets(descriptorPool, 1, &descriptorSet))
        return NULL;
    return descriptorSet;
}

bool DescriptorSetLayout::createDescriptorSets(std::shared_ptr<DescriptorPool> descriptorPool, uint32_t count, DescriptorSet** outDescriptorSets) {
    assert(descriptorPool->getDevice() == m_device);

    std::shared_ptr<DescriptorSetLayout> selfPointer = get(m_device, m_key); // shared_ptr from this
    assert(selfPointer.get() == this); // Sanity check

    for (int i = 0; i < count; ++i) {
        outDescriptorSets[i] = DescriptorSet::create(selfPointer, descriptorPool);
        if (outDescriptorSets[i] == NULL) {
            for (; i >= 0; --i) {
                delete outDescriptorSets[i];
                outDescriptorSets[i] = NULL;
            }
            return false;
        }
    }

    return true;
}

bool DescriptorSetLayout::createDescriptorSets(std::shared_ptr<DescriptorPool> descriptorPool, uint32_t count, std::shared_ptr<DescriptorSet>* outDescriptorSets) {
    DescriptorSet** tempDescriptorSets = new DescriptorSet*[count];

    if (!createDescriptorSets(descriptorPool, count, tempDescriptorSets)) {
        delete[] tempDescriptorSets;
        return false;
    }

    for (int i = 0; i < count; ++i) {
        outDescriptorSets[i] = std::shared_ptr<DescriptorSet>(tempDescriptorSets[i]);
    }
    delete[] tempDescriptorSets;
    return true;
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
    for (uint32_t i = 0; i < m_key.bindingCount; ++i) {
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

const GraphicsResource& DescriptorSetLayout::getResourceId() const {
    return m_resourceId;
}






DescriptorPool::DescriptorPool(std::weak_ptr<vkr::Device> device, vk::DescriptorPool descriptorPool, bool canFreeDescriptorSets):
        m_device(device),
        m_descriptorPool(descriptorPool),
        m_canFreeDescriptorSets(canFreeDescriptorSets),
        m_resourceId(GraphicsManager::nextResourceId()) {
}

DescriptorPool::~DescriptorPool() {
    if (m_canFreeDescriptorSets) {
        // TODO: should we keep a reference to all DescriptorSetLayouts allocated via this pool and free them here??
    } else {
        (**m_device).resetDescriptorPool(m_descriptorPool);
    }

    (**m_device).destroyDescriptorPool(m_descriptorPool);
}

DescriptorPool* DescriptorPool::create(const DescriptorPoolConfiguration& descriptorPoolConfiguration) {
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

    vk::DescriptorPool descriptorPool = (**descriptorPoolConfiguration.device.lock()).createDescriptorPool(createInfo);
    return new DescriptorPool(descriptorPoolConfiguration.device, descriptorPool, canFreeDescriptorSets);

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

const GraphicsResource& DescriptorPool::getResourceId() const {
    return m_resourceId;
}



DescriptorSet::DescriptorSet(std::weak_ptr<vkr::Device> device, std::weak_ptr<DescriptorPool> pool, std::weak_ptr<DescriptorSetLayout> layout, vk::DescriptorSet descriptorSet):
        m_device(device),
        m_pool(pool),
        m_layout(layout),
        m_descriptorSet(descriptorSet),
        m_resourceId(GraphicsManager::nextResourceId()) {
    //printf("Create DescriptorSet\n");
}

DescriptorSet::~DescriptorSet() {
    //printf("Destroy DescriptorSet\n");
    if (m_pool->canFreeDescriptorSets()) {
        (**m_device).freeDescriptorSets(m_pool->getDescriptorPool(), 1, &m_descriptorSet);
    }
}

DescriptorSet* DescriptorSet::create(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, std::weak_ptr<DescriptorPool> descriptorPool) {
    assert(!descriptorPool.expired());
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayout::get(descriptorPool.lock()->getDevice(), descriptorSetLayoutCreateInfo);
    return DescriptorSet::create(descriptorSetLayout, descriptorPool);
}

DescriptorSet* DescriptorSet::create(std::weak_ptr<DescriptorSetLayout> descriptorSetLayout, std::weak_ptr<DescriptorPool> descriptorPool) {
    assert(!descriptorSetLayout.expired() && !descriptorPool.expired());
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayoutPtr = descriptorSetLayout.lock();
    std::shared_ptr<DescriptorPool> descriptorPoolPtr = descriptorPool.lock();
    assert(descriptorSetLayoutPtr->getDevice() == descriptorPoolPtr->getDevice());

    const std::shared_ptr<vkr::Device>& device = descriptorPoolPtr->getDevice();

    vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;
    bool allocated = descriptorPoolPtr->allocate(descriptorSetLayoutPtr->getDescriptorSetLayout(), descriptorSet);
    assert(allocated && descriptorSet);

    return new DescriptorSet(device, descriptorPool, descriptorSetLayout, descriptorSet);
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

const GraphicsResource& DescriptorSet::getResourceId() const {
    return m_resourceId;
}



DescriptorSetWriter::DescriptorSetWriter(DescriptorSet* descriptorSet) :
        m_descriptorSet(descriptorSet) {
}

DescriptorSetWriter::DescriptorSetWriter(std::shared_ptr<DescriptorSet> descriptorSet) :
        m_descriptorSet(descriptorSet.get()) {
}

DescriptorSetWriter::DescriptorSetWriter(std::weak_ptr<DescriptorSet> descriptorSet):
        m_descriptorSet(descriptorSet.lock().get()) {
}

DescriptorSetWriter::~DescriptorSetWriter() {

}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
    assert(bindingIndex >= 0);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBinding(bindingIndex);

    assert(bindingInfo.descriptorCount == 1);

    size_t firstIndex = m_tempBufferInfo.size();
    m_tempBufferInfo.emplace_back(bufferInfo);

    vk::WriteDescriptorSet& write = m_writes.emplace_back();
    write.setDstSet(m_descriptorSet->getDescriptorSet());
    write.setDescriptorType(bindingInfo.descriptorType);
    write.setDstBinding(binding);
    write.setPBufferInfo((vk::DescriptorBufferInfo*)(firstIndex + 1));
    //write.setPBufferInfo(&m_tempBufferInfo.back());
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

DescriptorSetWriter& DescriptorSetWriter::writeTexelBufferView(uint32_t binding, const vk::BufferView& bufferView) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
    assert(bindingIndex >= 0);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBinding(bindingIndex);

    assert(bindingInfo.descriptorCount == 1);

    size_t firstIndex = m_tempBufferViews.size();
    m_tempBufferViews.emplace_back(bufferView);

    vk::WriteDescriptorSet& write = m_writes.emplace_back();
    write.setDstSet(m_descriptorSet->getDescriptorSet());
    write.setDescriptorType(bindingInfo.descriptorType);
    write.setDstBinding(binding);
    write.setPTexelBufferView((vk::BufferView*)(firstIndex + 1));
    //write.setPTexelBufferView(&m_tempBufferViews.back());
    write.setDescriptorCount(1);

    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeTexelBufferView(uint32_t binding, const BufferView* bufferView) {
    return writeTexelBufferView(binding, bufferView->getBufferView());
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::DescriptorImageInfo* imageInfos, uint32_t arrayCount, uint32_t arrayIndex) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
    assert(bindingIndex >= 0);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBinding(bindingIndex);

    assert(arrayCount > 0);
    assert((arrayIndex + arrayCount) <= bindingInfo.descriptorCount);

    size_t firstIndex = m_tempImageInfo.size();
    for (size_t i = 0; i < arrayCount; ++i) {
        m_tempImageInfo.emplace_back(imageInfos[i]);
    }

    vk::WriteDescriptorSet& write = m_writes.emplace_back();
    write.setDstSet(m_descriptorSet->getDescriptorSet());
    write.setDescriptorType(bindingInfo.descriptorType);
    write.setDstBinding(binding);
    // This is a horrible hack. The pointer directly has the value of firstIndex, which is later resolved to the correct pointer.
    // This is done because reallocation of m_tempImageInfo invalidates previous pointers.
    write.setPImageInfo((vk::DescriptorImageInfo*)(firstIndex + 1));
//    write.setPImageInfo(&m_tempImageInfo[firstIndex]);
    write.setDescriptorCount(arrayCount);
    write.setDstArrayElement(arrayIndex);

    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::DescriptorImageInfo& imageInfo, uint32_t arrayIndex) {
    return writeImage(binding, &imageInfo, 1, arrayIndex);
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, vk::Sampler* samplers, vk::ImageView* imageViews, vk::ImageLayout* imageLayouts, uint32_t arrayCount, uint32_t arrayIndex) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(samplers[i]);
        imageInfos[i].setImageView(imageViews[i]);
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayCount, arrayIndex);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout, uint32_t arrayIndex) {
    vk::DescriptorImageInfo imageInfo;
    imageInfo.setSampler(sampler);
    imageInfo.setImageView(imageView);
    imageInfo.setImageLayout(imageLayout);
    return writeImage(binding, &imageInfo, 1, arrayIndex);
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, Sampler** samplers, ImageView2D** imageViews, vk::ImageLayout* imageLayouts, uint32_t arrayCount, uint32_t arrayIndex) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(samplers[i]->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayCount, arrayIndex);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, Sampler* samplers, ImageView2D* imageViews, vk::ImageLayout imageLayouts, uint32_t arrayIndex) {
    return writeImage(binding, &samplers, &imageViews, &imageLayouts, 1, arrayIndex);
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, Texture2D** textures, vk::ImageLayout* imageLayouts, uint32_t arrayCount, uint32_t arrayIndex) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(textures[i]->getSampler()->getSampler());
        imageInfos[i].setImageView(textures[i]->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayCount, arrayIndex);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, Texture2D* texture, vk::ImageLayout imageLayout, uint32_t arrayIndex) {
    return writeImage(binding, &texture, &imageLayout, 1, arrayIndex);
}

bool DescriptorSetWriter::write() {
    const auto& device = m_descriptorSet->getDevice();
    for (auto& write : m_writes) {
        if (write.pImageInfo != nullptr) write.pImageInfo = &m_tempImageInfo[(size_t)write.pImageInfo - 1];
        if (write.pBufferInfo != nullptr) write.pBufferInfo = &m_tempBufferInfo[(size_t)write.pBufferInfo - 1];
        if (write.pTexelBufferView != nullptr) write.pTexelBufferView = &m_tempBufferViews[(size_t)write.pTexelBufferView - 1];
    }
    device->updateDescriptorSets(m_writes, {});
    return true;
}



DescriptorSetLayout::Key::Key(const vk::DescriptorSetLayoutCreateInfo& rhs) {
    bindingCount = rhs.bindingCount;
    pBindings = new vk::DescriptorSetLayoutBinding[rhs.bindingCount];

    vk::DescriptorSetLayoutBinding* bindings = const_cast<vk::DescriptorSetLayoutBinding*>(pBindings);

    vk::DescriptorSetLayoutBinding* sortStart = NULL;

    // Ensure bindings array is sorted
    int prevBinding = -1;
    for (int i = 0; i < bindingCount; ++i) {
        bindings[i] = rhs.pBindings[i];

        if ((int)bindings[i].binding > prevBinding) {
            prevBinding = bindings[i].binding;

        } else if (sortStart == NULL) {
            // Bindings need to be sorted starting from the previous one.
            sortStart = &bindings[i - 1];
        }
    }

    if (sortStart != NULL) {
        std::sort(sortStart, bindings + bindingCount, [](vk::DescriptorSetLayoutBinding lhs, vk::DescriptorSetLayoutBinding rhs) {
            return lhs.binding < rhs.binding;
        });
    }
}

DescriptorSetLayout::Key::Key(const Key& copy):
        Key(static_cast<vk::DescriptorSetLayoutCreateInfo>(copy)){
}

DescriptorSetLayout::Key::Key(Key&& move) {
    bindingCount = std::exchange(move.bindingCount, 0);
    pBindings = std::exchange(move.pBindings, nullptr);
}

DescriptorSetLayout::Key::~Key() {
    delete[] pBindings;
    pBindings = NULL;
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

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(std::weak_ptr<vkr::Device> device, vk::DescriptorSetLayoutCreateFlags flags):
        m_device(device),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(vk::DescriptorSetLayoutCreateFlags flags):
        m_device(Application::instance()->graphics()->getDevice()),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::~DescriptorSetLayoutBuilder() {
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addUniformBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const size_t& sizeBytes, const bool& dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout UniformBlock binding %d - The binding is already added\n", binding);
		assert(false);
	}
	if (sizeBytes % 4 != 0) {
		printf("Unable to add DescriptorSetLayout UniformBlock binding %d - Uniform block size must be a multiple of 4 bytes\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(dynamic ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer);
    bindingInfo.setDescriptorCount(1);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const size_t& sizeBytes, const bool& dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout StorageBlock binding %d - The binding is already added\n", binding);
		assert(false);
	}
	if (sizeBytes % 4 != 0) {
		printf("Unable to add DescriptorSetLayout StorageBlock binding %d - Storage block size must be a multiple of 4 bytes\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(dynamic ? vk::DescriptorType::eStorageBufferDynamic : vk::DescriptorType::eStorageBuffer);
    bindingInfo.setDescriptorCount(1);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}


DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageTexelBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout StorageTexelBuffer binding %d - The binding is already added\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eStorageTexelBuffer);
    bindingInfo.setDescriptorCount(1);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const size_t& arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout Sampler binding %d - The binding is already added\n", binding);
		assert(false);
	}
	if (arraySize == 0) {
		printf("Unable to add DescriptorSetLayout Sampler binding %d - Array size must not be zero\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eSampler);
    bindingInfo.setDescriptorCount(arraySize);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampledImage(const uint32_t &binding, const vk::ShaderStageFlags &shaderStages, const size_t &arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout SampledImage binding %d - The binding is already added\n", binding);
		assert(false);
	}
	if (arraySize == 0) {
		printf("Unable to add DescriptorSetLayout SampledImage binding %d - Array size must not be zero\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eSampledImage);
    bindingInfo.setDescriptorCount(arraySize);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addCombinedImageSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const size_t& arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout CombinedImageSampler binding %d - The binding is already added\n", binding);
		assert(false);
	}
	if (arraySize == 0) {
		printf("Unable to add DescriptorSetLayout CombinedImageSampler binding %d - Array size must not be zero\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    bindingInfo.setDescriptorCount(arraySize);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(NULL);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayoutBuilder::build() {
    assert(!m_device.expired());

    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_bindings.size());
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        bindings.emplace_back(it->second);
    }

    vk::DescriptorSetLayoutCreateInfo createInfo;
    createInfo.setFlags(m_flags);
    createInfo.setBindings(bindings);

    reset();

    return DescriptorSetLayout::get(m_device, createInfo);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::reset(vk::DescriptorSetLayoutCreateFlags flags) {
    m_flags = flags;
    m_bindings.clear();
    return *this;
}
