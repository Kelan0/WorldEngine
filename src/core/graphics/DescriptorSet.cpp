#include <utility>

#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/BufferView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/ImageView.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"

DescriptorSetLayout::Cache DescriptorSetLayout::s_descriptorSetLayoutCache;


DescriptorSetLayout::DescriptorSetLayout(const std::weak_ptr<vkr::Device>& device, const vk::DescriptorSetLayout& descriptorSetLayout, Key key):
        m_device(device),
        m_descriptorSetLayout(descriptorSetLayout),
        m_key(std::move(key)),
        m_resourceId(GraphicsManager::nextResourceId()) {
    //printf("Create DescriptorSetLayout\n");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    //printf("Destroy DescriptorSetLayout\n");
    (**m_device).destroyDescriptorSetLayout(m_descriptorSetLayout);
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayout::get(const std::weak_ptr<vkr::Device>& device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const char* name) {
    Key key(descriptorSetLayoutCreateInfo);

#if _DEBUG
    // Check for duplicated bindings
	int lastBinding = -1;
	for (uint32_t i = 0; i < key.bindingCount; ++i) {
		int binding = (int)key.pBindings[i].binding;
		if (binding == lastBinding) {
			printf("Descriptor set layout has duplicated bindings\n");
			assert(false);
			return nullptr;
		}
	}
#endif

    auto it = s_descriptorSetLayoutCache.find(key);
    if (it == s_descriptorSetLayoutCache.end() || it->second.expired()) {
        vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        const vk::Device& dvc = **device.lock();
        vk::Result result = dvc.createDescriptorSetLayout(&descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
        if (result != vk::Result::eSuccess) {
            printf("Failed to create descriptor set layout: %s\n", vk::to_string(result).c_str());
            return nullptr;
        }
        Engine::graphics()->setObjectName(dvc, (uint64_t)(VkDescriptorSetLayout)descriptorSetLayout, vk::ObjectType::eDescriptorSetLayout, name);

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

DescriptorSet* DescriptorSetLayout::createDescriptorSet(const std::shared_ptr<DescriptorPool>& descriptorPool, const char* name) {
    DescriptorSet* descriptorSet = nullptr;
    if (!createDescriptorSets(descriptorPool, 1, &descriptorSet, name))
        return nullptr;
    return descriptorSet;
}

bool DescriptorSetLayout::createDescriptorSets(const std::shared_ptr<DescriptorPool>& descriptorPool, const uint32_t& count, DescriptorSet** outDescriptorSets, const char* name) {
    assert(descriptorPool->getDevice() == m_device);

    std::shared_ptr<DescriptorSetLayout> selfPointer = DescriptorSetLayout::get(m_device, m_key, nullptr); // shared_ptr from this
    assert(selfPointer.get() == this); // Sanity check

    for (uint32_t i = 0; i < count; ++i) {
        outDescriptorSets[i] = DescriptorSet::create(selfPointer, descriptorPool, name);
        if (outDescriptorSets[i] == nullptr) {
            for (; i >= 0; --i) {
                delete outDescriptorSets[i];
                outDescriptorSets[i] = nullptr;
            }
            return false;
        }
    }

    return true;
}

bool DescriptorSetLayout::createDescriptorSets(const std::shared_ptr<DescriptorPool>& descriptorPool, const uint32_t& count, std::shared_ptr<DescriptorSet>* outDescriptorSets, const char* name) {
    DescriptorSet** tempDescriptorSets = new DescriptorSet*[count];

    if (!createDescriptorSets(descriptorPool, count, tempDescriptorSets, name)) {
        delete[] tempDescriptorSets;
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        outDescriptorSets[i] = std::shared_ptr<DescriptorSet>(tempDescriptorSets[i]);
    }
    delete[] tempDescriptorSets;
    return true;
}

const std::shared_ptr<vkr::Device>& DescriptorSetLayout::getDevice() const {
    return m_device;
}

const vk::DescriptorSetLayout& DescriptorSetLayout::getDescriptorSetLayout() const {
    return m_descriptorSetLayout;
}

std::vector<vk::DescriptorSetLayoutBinding> DescriptorSetLayout::getBindings() const {
    return std::vector<vk::DescriptorSetLayoutBinding>(m_key.pBindings, m_key.pBindings + m_key.bindingCount);
}

bool DescriptorSetLayout::hasBinding(const uint32_t& binding) const {
    return findBindingIndex(binding) >= 0;
}

int32_t DescriptorSetLayout::findBindingIndex(const uint32_t& binding) const {
    for (uint32_t i = 0; i < m_key.bindingCount; ++i) {
        if (m_key.pBindings[i].binding == binding) {
            return i;
        }
    }

    return -1;
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::findBinding(const uint32_t& binding) const {
    int index = findBindingIndex(binding);
    assert(index >= 0);
    return getBinding(index);
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::getBinding(const int32_t& index) const {
    assert(index >= 0 && (uint32_t)index < m_key.bindingCount);
    return m_key.pBindings[index];
}

const uint32_t& DescriptorSetLayout::getBindingCount() const {
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






DescriptorPool::DescriptorPool(const std::weak_ptr<vkr::Device>& device, vk::DescriptorPool descriptorPool, bool canFreeDescriptorSets):
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

DescriptorPool* DescriptorPool::create(const DescriptorPoolConfiguration& descriptorPoolConfiguration, const char* name) {
    std::vector<vk::DescriptorPoolSize> poolSizes;

    for (const auto& entry : descriptorPoolConfiguration.poolSizes) {
        if (entry.second == 0)
            continue; // Skip zero-size pools

        vk::DescriptorPoolSize& poolSize = poolSizes.emplace_back();
        poolSize.setType(entry.first);
        poolSize.setDescriptorCount(entry.second);
    }

    vk::DescriptorPoolCreateInfo createInfo;
    createInfo.setFlags(descriptorPoolConfiguration.flags);
    createInfo.setMaxSets(descriptorPoolConfiguration.maxSets);
    createInfo.setPoolSizes(poolSizes);

    bool canFreeDescriptorSets = (bool)(createInfo.flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

    const vk::Device& device = **descriptorPoolConfiguration.device.lock();

    vk::DescriptorPool descriptorPool = device.createDescriptorPool(createInfo);

    Engine::graphics()->setObjectName(device, (uint64_t)(VkDescriptorPool)descriptorPool, vk::ObjectType::eDescriptorPool, name);

    return new DescriptorPool(descriptorPoolConfiguration.device, descriptorPool, canFreeDescriptorSets);

}

const std::shared_ptr<vkr::Device>& DescriptorPool::getDevice() const {
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

const bool& DescriptorPool::canFreeDescriptorSets() const {
    return m_canFreeDescriptorSets;
}

const GraphicsResource& DescriptorPool::getResourceId() const {
    return m_resourceId;
}



DescriptorSet::DescriptorSet(const std::weak_ptr<vkr::Device>& device, const std::weak_ptr<DescriptorPool>& pool, const std::weak_ptr<DescriptorSetLayout>& layout, const vk::DescriptorSet& descriptorSet):
        m_device(device),
        m_pool(pool),
        m_layout(layout),
        m_descriptorSet(descriptorSet),
        m_resourceId(GraphicsManager::nextResourceId()) {
    //printf("Create DescriptorSet\n");
}

DescriptorSet::DescriptorSet(DescriptorSet&& move) noexcept :
        m_device(std::exchange(move.m_device, nullptr)),
        m_pool(std::exchange(move.m_pool, nullptr)),
        m_layout(std::exchange(move.m_layout, nullptr)),
        m_descriptorSet(std::exchange(move.m_descriptorSet, VK_NULL_HANDLE)),
        m_resourceId(std::exchange(move.m_resourceId, GraphicsResource{})) {
}

DescriptorSet::~DescriptorSet() {
    //printf("Destroy DescriptorSet\n");
    if (m_pool->canFreeDescriptorSets()) {
        (**m_device).freeDescriptorSets(m_pool->getDescriptorPool(), 1, &m_descriptorSet);
    }
}

DescriptorSet* DescriptorSet::create(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const std::weak_ptr<DescriptorPool>& descriptorPool, const char* name, const char* layoutName) {
    assert(!descriptorPool.expired());
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayout::get(descriptorPool.lock()->getDevice(), descriptorSetLayoutCreateInfo, layoutName);
    return DescriptorSet::create(descriptorSetLayout, descriptorPool, name);
}

DescriptorSet* DescriptorSet::create(const std::weak_ptr<DescriptorSetLayout>& descriptorSetLayout, const std::weak_ptr<DescriptorPool>& descriptorPool, const char* name) {
    assert(!descriptorSetLayout.expired() && !descriptorPool.expired());
    std::shared_ptr<DescriptorSetLayout> descriptorSetLayoutPtr = descriptorSetLayout.lock();
    std::shared_ptr<DescriptorPool> descriptorPoolPtr = descriptorPool.lock();
    assert(descriptorSetLayoutPtr->getDevice() == descriptorPoolPtr->getDevice());

    const std::shared_ptr<vkr::Device>& device = descriptorPoolPtr->getDevice();

    vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;
    bool allocated = descriptorPoolPtr->allocate(descriptorSetLayoutPtr->getDescriptorSetLayout(), descriptorSet);

    Engine::graphics()->setObjectName(**device, (uint64_t)(VkDescriptorSet)descriptorSet, vk::ObjectType::eDescriptorSet, name);

    assert(allocated && descriptorSet);

    return new DescriptorSet(device, descriptorPool, descriptorSetLayout, descriptorSet);
}

const vk::DescriptorSet& DescriptorSet::getDescriptorSet() const {
    return m_descriptorSet;
}

const std::shared_ptr<vkr::Device>& DescriptorSet::getDevice() const {
    return m_device;
}

const std::shared_ptr<DescriptorPool>& DescriptorSet::getPool() const {
    return m_pool;
}

const std::shared_ptr<DescriptorSetLayout>& DescriptorSet::getLayout() const {
    return m_layout;
}

const GraphicsResource& DescriptorSet::getResourceId() const {
    return m_resourceId;
}



DescriptorSetWriter::DescriptorSetWriter(DescriptorSet* descriptorSet) :
        m_descriptorSet(descriptorSet) {
}

DescriptorSetWriter::DescriptorSetWriter(const std::shared_ptr<DescriptorSet>& descriptorSet) :
        m_descriptorSet(descriptorSet.get()) {
}

DescriptorSetWriter::DescriptorSetWriter(const std::weak_ptr<DescriptorSet>& descriptorSet):
        m_descriptorSet(descriptorSet.lock().get()) {
}

DescriptorSetWriter::~DescriptorSetWriter() = default;

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(const uint32_t& binding, const vk::DescriptorBufferInfo& bufferInfo) {
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

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(const uint32_t& binding, const vk::Buffer& buffer, const vk::DeviceSize& offset, const vk::DeviceSize& range) {
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(buffer);
    bufferInfo.setOffset(offset);
    bufferInfo.setRange(range);
    return writeBuffer(binding, bufferInfo);
}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(const uint32_t& binding, const Buffer* buffer, const vk::DeviceSize& offset, const vk::DeviceSize& range) {
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(buffer->getBuffer());
    bufferInfo.setOffset(offset);
    bufferInfo.setRange(range);
    return writeBuffer(binding, bufferInfo);
}

DescriptorSetWriter& DescriptorSetWriter::writeTexelBufferView(const uint32_t& binding, const vk::BufferView& bufferView) {
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

DescriptorSetWriter& DescriptorSetWriter::writeTexelBufferView(const uint32_t& binding, const BufferView* bufferView) {
    return writeTexelBufferView(binding, bufferView->getBufferView());
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const vk::DescriptorImageInfo* imageInfos, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
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

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const vk::DescriptorImageInfo& imageInfo, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i] = imageInfo; // Duplicate this image the specified number of times
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const vk::Sampler* samplers, const vk::ImageView* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(samplers[i]);
        imageInfos[i].setImageView(imageViews[i]);
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const vk::Sampler& sampler, const vk::ImageView& imageView, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(sampler);
        imageInfos[i].setImageView(imageView);
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Sampler* const* samplers, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(samplers[i]->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}
DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(sampler->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(sampler->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}


DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* imageView, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(sampler->getSampler());
        imageInfos[i].setImageView(imageView->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Texture* const* textures, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    assert(arrayCount > 0);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(textures[i]->getSampler()->getSampler());
        imageInfos[i].setImageView(textures[i]->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}
DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Texture* const* textures, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    assert(arrayCount > 0);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(textures[i]->getSampler()->getSampler());
        imageInfos[i].setImageView(textures[i]->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(const uint32_t& binding, const Texture* texture, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount) {
    assert(arrayCount > 0);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i].setSampler(texture->getSampler()->getSampler());
        imageInfos[i].setImageView(texture->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

bool DescriptorSetWriter::write() {
    if (!m_writes.empty()) {
        const auto& device = m_descriptorSet->getDevice();
        for (auto& write: m_writes) {
            if (write.pImageInfo != nullptr) write.pImageInfo = &m_tempImageInfo[(size_t) write.pImageInfo - 1];
            if (write.pBufferInfo != nullptr) write.pBufferInfo = &m_tempBufferInfo[(size_t) write.pBufferInfo - 1];
            if (write.pTexelBufferView != nullptr) write.pTexelBufferView = &m_tempBufferViews[(size_t) write.pTexelBufferView - 1];
        }
        device->updateDescriptorSets(m_writes, {});
    }
    return true;
}



DescriptorSetLayout::Key::Key(const vk::DescriptorSetLayoutCreateInfo& rhs) {
    bindingCount = rhs.bindingCount;
    pBindings = new vk::DescriptorSetLayoutBinding[rhs.bindingCount];

    vk::DescriptorSetLayoutBinding* bindings = const_cast<vk::DescriptorSetLayoutBinding*>(pBindings);

    vk::DescriptorSetLayoutBinding* sortStart = nullptr;

    // Ensure bindings array is sorted
    int64_t prevBinding = -1;
    for (uint32_t i = 0; i < bindingCount; ++i) {
        bindings[i] = rhs.pBindings[i];

        if ((int)bindings[i].binding > prevBinding) {
            prevBinding = bindings[i].binding;

        } else if (sortStart == nullptr) {
            // Bindings need to be sorted starting from the previous one.
            sortStart = &bindings[i - 1];
        }
    }

    if (sortStart != nullptr) {
        std::sort(sortStart, bindings + bindingCount, [](vk::DescriptorSetLayoutBinding lhs, vk::DescriptorSetLayoutBinding rhs) {
            return lhs.binding < rhs.binding;
        });
    }
}

DescriptorSetLayout::Key::Key(const Key& copy):
        Key(static_cast<vk::DescriptorSetLayoutCreateInfo>(copy)){
}

DescriptorSetLayout::Key::Key(Key&& move) noexcept {
    bindingCount = std::exchange(move.bindingCount, 0);
    pBindings = std::exchange(move.pBindings, nullptr);
}

DescriptorSetLayout::Key::~Key() {
    delete[] pBindings;
    pBindings = nullptr;
}

bool DescriptorSetLayout::Key::operator==(const DescriptorSetLayout::Key& rhs) const {
    if (flags != rhs.flags)
        return false;
    if (bindingCount != rhs.bindingCount)
        return false;
    for (uint32_t i = 0; i < bindingCount; ++i)
        if (pBindings[i] != rhs.pBindings[i]) // vk::DescriptorSetLayoutBinding has a valid operator== definition
            return false;

    return true;
}

size_t DescriptorSetLayout::KeyHasher::operator()(const DescriptorSetLayout::Key& descriptorSetLayoutKey) const {
    size_t s = 0;
    std::hash_combine(s, (uint32_t)descriptorSetLayoutKey.flags);
    for (uint32_t i = 0; i < descriptorSetLayoutKey.bindingCount; ++i) {
        std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].binding);
        std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].descriptorType);
        std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].descriptorCount);
        std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].stageFlags);
        std::hash_combine(s, descriptorSetLayoutKey.pBindings[i].pImmutableSamplers);
    }
    return s;
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(const std::weak_ptr<vkr::Device>& device, const vk::DescriptorSetLayoutCreateFlags& flags):
        m_device(device),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(const vk::DescriptorSetLayoutCreateFlags& flags):
        m_device(Engine::graphics()->getDevice()),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::~DescriptorSetLayoutBuilder() = default;

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addUniformBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const bool& dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout UniformBlock binding %d - The binding is already added\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(dynamic ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eUniformBuffer);
    bindingInfo.setDescriptorCount(1);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const bool& dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout StorageBlock binding %d - The binding is already added\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(dynamic ? vk::DescriptorType::eStorageBufferDynamic : vk::DescriptorType::eStorageBuffer);
    bindingInfo.setDescriptorCount(1);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(nullptr);
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
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize) {
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
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampledImage(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize) {
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
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addCombinedImageSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize) {
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
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addInputAttachment(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
		printf("Unable to add DescriptorSetLayout InputAttachment binding %d - The binding is already added\n", binding);
		assert(false);
	}
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eInputAttachment);
    bindingInfo.setDescriptorCount(arraySize);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageImage(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        printf("Unable to add DescriptorSetLayout StorageImage binding %d - The binding is already added\n", binding);
        assert(false);
    }
    if (arraySize == 0) {
        printf("Unable to add DescriptorSetLayout StorageImage binding %d - Array size must not be zero\n", binding);
        assert(false);
    }
#endif
    vk::DescriptorSetLayoutBinding bindingInfo;
    bindingInfo.setBinding(binding);
    bindingInfo.setDescriptorType(vk::DescriptorType::eStorageImage);
    bindingInfo.setDescriptorCount(arraySize);
    bindingInfo.setStageFlags(shaderStages);
    bindingInfo.setPImmutableSamplers(nullptr);
    m_bindings.insert(std::make_pair(binding, bindingInfo));
    return *this;
}

std::shared_ptr<DescriptorSetLayout> DescriptorSetLayoutBuilder::build(const char* name) {
    assert(!m_device.expired());

    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(m_bindings.size());
    for (auto& binding : m_bindings) {
        bindings.emplace_back(binding.second);
    }

    vk::DescriptorSetLayoutCreateInfo createInfo;
    createInfo.setFlags(m_flags);
    createInfo.setBindings(bindings);

    reset();

    return DescriptorSetLayout::get(m_device, createInfo, name);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::reset(vk::DescriptorSetLayoutCreateFlags flags) {
    m_flags = flags;
    m_bindings.clear();
    return *this;
}
