#include <utility>

#include "core/graphics/DescriptorSet.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/BufferView.h"
#include "core/graphics/Texture.h"
#include "core/graphics/ImageView.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"
#include "core/util/Logger.h"

DescriptorSetLayout::Cache DescriptorSetLayout::s_descriptorSetLayoutCache;


DescriptorSetLayout::DescriptorSetLayout(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayout& descriptorSetLayout, Key key, const std::string& name):
        GraphicsResource(ResourceType_DescriptorSetLayout, device, name),
        m_descriptorSetLayout(descriptorSetLayout),
        m_key(std::move(key)) {
    //printf("Create DescriptorSetLayout\n");
}

DescriptorSetLayout::~DescriptorSetLayout() {
    //printf("Destroy DescriptorSetLayout\n");
    (**m_device).destroyDescriptorSetLayout(m_descriptorSetLayout);
}

SharedResource<DescriptorSetLayout> DescriptorSetLayout::get(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const std::string& name) {
    Key key(descriptorSetLayoutCreateInfo);

#if _DEBUG
    // Check for duplicated bindings
    int lastBinding = -1;
    for (uint32_t i = 0; i < key.bindingCount; ++i) {
        int binding = (int)key.pBindings[i].binding;
        if (binding == lastBinding) {
            LOG_FATAL("Descriptor set layout has duplicated bindings");
            assert(false);
            return nullptr;
        }
    }
#endif

    auto it = s_descriptorSetLayoutCache.find(key);
    if (it == s_descriptorSetLayoutCache.end() || it->second.expired()) {
        vk::DescriptorSetLayout descriptorSetLayout = nullptr;
        const vk::Device& dvc = **device.lock(name);
        vk::Result result = dvc.createDescriptorSetLayout(&descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
        if (result != vk::Result::eSuccess) {
            LOG_ERROR("Failed to create descriptor set layout: %s", vk::to_string(result).c_str());
            return nullptr;
        }
        Engine::graphics()->setObjectName(dvc, (uint64_t)(VkDescriptorSetLayout)descriptorSetLayout, vk::ObjectType::eDescriptorSetLayout, name);

        SharedResource<DescriptorSetLayout> retDescriptorSetLayout(new DescriptorSetLayout(device, descriptorSetLayout, key, name), name);
        s_descriptorSetLayoutCache.insert(std::make_pair(key, WeakResource<DescriptorSetLayout>(retDescriptorSetLayout)));
        return retDescriptorSetLayout;

    } else {
        return SharedResource<DescriptorSetLayout>(it->second, name);
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
        LOG_WARN("Clearing descriptor set layout cache. %d descriptor set layouts have external references and will not be freed", count);
    }
#endif

    s_descriptorSetLayoutCache.clear();
}

DescriptorSet* DescriptorSetLayout::createDescriptorSet(const SharedResource<DescriptorPool>& descriptorPool, const std::string& name) {
    DescriptorSet* descriptorSet = nullptr;
    if (!createDescriptorSets(descriptorPool, 1, &descriptorSet, name))
        return nullptr;
    return descriptorSet;
}

bool DescriptorSetLayout::createDescriptorSets(const SharedResource<DescriptorPool>& descriptorPool, uint32_t count, DescriptorSet** outDescriptorSets, const std::string& name) {
    assert(descriptorPool->getDevice() == m_device);

    SharedResource<DescriptorSetLayout> selfPointer = DescriptorSetLayout::get(m_device, m_key, m_name); // shared_ptr from this
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

bool DescriptorSetLayout::createDescriptorSets(const SharedResource<DescriptorPool>& descriptorPool, uint32_t count, SharedResource<DescriptorSet>* outDescriptorSets, const std::string& name) {
    DescriptorSet** tempDescriptorSets = new DescriptorSet* [count];

    if (!createDescriptorSets(descriptorPool, count, tempDescriptorSets, name)) {
        delete[] tempDescriptorSets;
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        outDescriptorSets[i] = SharedResource<DescriptorSet>(tempDescriptorSets[i], name);
    }
    delete[] tempDescriptorSets;
    return true;
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

int32_t DescriptorSetLayout::findBindingIndex(uint32_t binding) const {
    for (uint32_t i = 0; i < m_key.bindingCount; ++i) {
        if (m_key.pBindings[i].binding == binding) {
            return i;
        }
    }

    return -1;
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::getBinding(uint32_t binding) const {
    int index = findBindingIndex(binding);
    assert(index >= 0);
    return getBindingByIndex(index);
}

const vk::DescriptorSetLayoutBinding& DescriptorSetLayout::getBindingByIndex(int32_t index) const {
    assert(index >= 0 && (uint32_t)index < m_key.bindingCount);
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


DescriptorPool::DescriptorPool(const WeakResource<vkr::Device>& device, vk::DescriptorPool descriptorPool, const DescriptorPoolConfiguration& config, const std::string& name):
        GraphicsResource(ResourceType_DescriptorPool, device, name),
        m_descriptorPool(descriptorPool),
        m_descriptorPoolSizes(config.poolSizes),
        m_flags(config.flags) {
}

DescriptorPool::~DescriptorPool() {
    if (canFreeDescriptorSets()) {
        // TODO: should we keep a reference to all DescriptorSetLayouts allocated via this pool and free them here??
    } else {
        (**m_device).resetDescriptorPool(m_descriptorPool);
    }

    (**m_device).destroyDescriptorPool(m_descriptorPool);
}

DescriptorPool* DescriptorPool::create(const DescriptorPoolConfiguration& descriptorPoolConfiguration, const std::string& name) {
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

    const vk::Device& device = **descriptorPoolConfiguration.device.lock(name);

    vk::DescriptorPool descriptorPool = device.createDescriptorPool(createInfo);

    Engine::graphics()->setObjectName(device, (uint64_t)(VkDescriptorPool)descriptorPool, vk::ObjectType::eDescriptorPool, name);

    return new DescriptorPool(descriptorPoolConfiguration.device, descriptorPool, descriptorPoolConfiguration, name);
}

const vk::DescriptorPool& DescriptorPool::getDescriptorPool() const {
    return m_descriptorPool;
}

vk::Result DescriptorPool::allocate(const vk::DescriptorSetLayout& descriptorSetLayout, vk::DescriptorSet& outDescriptorSet) const {

    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.setDescriptorPool(m_descriptorPool);
    allocateInfo.setDescriptorSetCount(1);
    allocateInfo.setPSetLayouts(&descriptorSetLayout);
    vk::Result result = (**m_device).allocateDescriptorSets(&allocateInfo, &outDescriptorSet);
    return result;
}

void DescriptorPool::free(const vk::DescriptorSet& descriptorSet) {
    (**m_device).freeDescriptorSets(m_descriptorPool, 1, &descriptorSet);
}

bool DescriptorPool::canFreeDescriptorSets() const {
    return (bool)(m_flags & vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
}


DescriptorSet::DescriptorSet(const WeakResource<vkr::Device>& device, const WeakResource<DescriptorPool>& pool, const WeakResource<DescriptorSetLayout>& layout, const vk::DescriptorSet& descriptorSet, const std::string& name):
        GraphicsResource(ResourceType_DescriptorSet, device, name),
        m_pool(pool, name),
        m_layout(layout, name),
        m_descriptorSet(descriptorSet) {
    //printf("Create DescriptorSet\n");
}

DescriptorSet::DescriptorSet(DescriptorSet&& move) noexcept :
        GraphicsResource(std::move(move)),
        m_pool(std::exchange(move.m_pool, nullptr)),
        m_layout(std::exchange(move.m_layout, nullptr)),
        m_descriptorSet(std::exchange(move.m_descriptorSet, nullptr)) {
}

DescriptorSet::~DescriptorSet() {
    //printf("Destroy DescriptorSet\n");
    if (m_pool->canFreeDescriptorSets()) {
        (**m_device).freeDescriptorSets(m_pool->getDescriptorPool(), 1, &m_descriptorSet);
    }
}

DescriptorSet* DescriptorSet::create(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const WeakResource<DescriptorPool>& descriptorPool, const std::string& name, const std::string& layoutName) {
    assert(!descriptorPool.expired());
    const SharedResource<DescriptorSetLayout>& descriptorSetLayout = DescriptorSetLayout::get(descriptorPool.lock(name)->getDevice(), descriptorSetLayoutCreateInfo, layoutName);
    return DescriptorSet::create(descriptorSetLayout, descriptorPool, name);
}

DescriptorSet* DescriptorSet::create(const WeakResource<DescriptorSetLayout>& descriptorSetLayout, const WeakResource<DescriptorPool>& descriptorPool, const std::string& name) {
    assert(!descriptorSetLayout.expired() && !descriptorPool.expired());
    SharedResource<DescriptorSetLayout> descriptorSetLayoutPtr = descriptorSetLayout.lock(name);
    SharedResource<DescriptorPool> descriptorPoolPtr = descriptorPool.lock(name);
    assert(descriptorSetLayoutPtr->getDevice() == descriptorPoolPtr->getDevice());

    const SharedResource<vkr::Device>& device = descriptorPoolPtr->getDevice();

    vk::DescriptorSet descriptorSet = nullptr;
    vk::Result result = descriptorPoolPtr->allocate(descriptorSetLayoutPtr->getDescriptorSetLayout(), descriptorSet);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to allocate descriptor set \"%s\": %s", name.c_str(), vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(**device, (uint64_t)(VkDescriptorSet)descriptorSet, vk::ObjectType::eDescriptorSet, name);

    return new DescriptorSet(device, descriptorPool, descriptorSetLayout, descriptorSet, name);
}

const vk::DescriptorSet& DescriptorSet::getDescriptorSet() const {
    return m_descriptorSet;
}

const SharedResource<DescriptorPool>& DescriptorSet::getPool() const {
    return m_pool;
}

const SharedResource<DescriptorSetLayout>& DescriptorSet::getLayout() const {
    return m_layout;
}


DescriptorSetWriter::DescriptorSetWriter(DescriptorSet* descriptorSet):
        m_descriptorSet(descriptorSet) {
}

DescriptorSetWriter::DescriptorSetWriter(const SharedResource<DescriptorSet>& descriptorSet):
        m_descriptorSet(descriptorSet.get()) {
}

DescriptorSetWriter::DescriptorSetWriter(const WeakResource<DescriptorSet>& descriptorSet):
        m_descriptorSet(descriptorSet.lock("DescriptorSetWriter").get()) {
}

DescriptorSetWriter::~DescriptorSetWriter() = default;

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, const vk::DescriptorBufferInfo& bufferInfo) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
    assert(bindingIndex >= 0);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBindingByIndex(bindingIndex);

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

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, const vk::Buffer& buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(buffer);
    bufferInfo.setOffset(offset);
    bufferInfo.setRange(range);
    return writeBuffer(binding, bufferInfo);
}

DescriptorSetWriter& DescriptorSetWriter::writeBuffer(uint32_t binding, const Buffer* buffer, vk::DeviceSize offset, vk::DeviceSize range) {
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setBuffer(buffer->getBuffer());
    bufferInfo.setOffset(offset);
    bufferInfo.setRange(range);
    return writeBuffer(binding, bufferInfo);
}

DescriptorSetWriter& DescriptorSetWriter::writeTexelBufferView(uint32_t binding, const vk::BufferView& bufferView) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
    assert(bindingIndex >= 0);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBindingByIndex(bindingIndex);

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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::DescriptorImageInfo* imageInfos, uint32_t arrayIndex, uint32_t arrayCount) {
    int bindingIndex = m_descriptorSet->getLayout()->findBindingIndex(binding);
#if _DEBUG
    bool hasBinding = bindingIndex >= 0;
    assert(hasBinding);
#endif
    assert(imageInfos != nullptr);

    const auto& bindingInfo = m_descriptorSet->getLayout()->getBindingByIndex(bindingIndex);

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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::DescriptorImageInfo& imageInfo, uint32_t arrayIndex, uint32_t arrayCount) {
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        imageInfos[i] = imageInfo; // Duplicate this image the specified number of times
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::Sampler* samplers, const vk::ImageView* imageViews, const vk::ImageLayout* imageLayouts, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(samplers != nullptr);
    assert(imageViews != nullptr);
    assert(imageLayouts != nullptr);
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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const vk::Sampler& sampler, const vk::ImageView& imageView, const vk::ImageLayout& imageLayout, uint32_t arrayIndex, uint32_t arrayCount) {
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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Sampler* const* samplers, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(samplers != nullptr);
    assert(imageViews != nullptr);
    assert(imageLayouts != nullptr);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        assert(samplers[i] != nullptr);
        assert(imageViews[i] != nullptr);
        imageInfos[i].setSampler(samplers[i]->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(sampler != nullptr);
    assert(imageViews != nullptr);
    assert(imageLayouts != nullptr);
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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout& imageLayout, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(sampler != nullptr);
    assert(imageViews != nullptr);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        assert(imageViews[i] != nullptr);
        imageInfos[i].setSampler(sampler->getSampler());
        imageInfos[i].setImageView(imageViews[i]->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}


DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Sampler* sampler, const ImageView* imageView, const vk::ImageLayout& imageLayout, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(sampler != nullptr);
    assert(imageView != nullptr);
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

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Texture* const* textures, const vk::ImageLayout* imageLayouts, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(textures != nullptr);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        assert(textures[i] != nullptr);
        assert(textures[i]->getSampler() != nullptr);
        assert(textures[i]->getImageView() != nullptr);
        imageInfos[i].setSampler(textures[i]->getSampler()->getSampler());
        imageInfos[i].setImageView(textures[i]->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayouts[i]);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Texture* const* textures, const vk::ImageLayout& imageLayout, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(textures != nullptr);
    vk::DescriptorImageInfo* imageInfos = new vk::DescriptorImageInfo[arrayCount];
    for (uint32_t i = 0; i < arrayCount; ++i) {
        assert(textures[i] != nullptr);
        assert(textures[i]->getSampler() != nullptr);
        assert(textures[i]->getImageView() != nullptr);
        imageInfos[i].setSampler(textures[i]->getSampler()->getSampler());
        imageInfos[i].setImageView(textures[i]->getImageView()->getImageView());
        imageInfos[i].setImageLayout(imageLayout);
    }
    writeImage(binding, imageInfos, arrayIndex, arrayCount);
    delete[] imageInfos;
    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::writeImage(uint32_t binding, const Texture* texture, const vk::ImageLayout& imageLayout, uint32_t arrayIndex, uint32_t arrayCount) {
    assert(arrayCount > 0);
    assert(texture != nullptr);
    assert(texture->getSampler() != nullptr);
    assert(texture->getImageView() != nullptr);
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
        for (auto& write : m_writes) {
            if (write.pImageInfo != nullptr) write.pImageInfo = &m_tempImageInfo[(size_t)write.pImageInfo - 1];
            if (write.pBufferInfo != nullptr) write.pBufferInfo = &m_tempBufferInfo[(size_t)write.pBufferInfo - 1];
            if (write.pTexelBufferView != nullptr) write.pTexelBufferView = &m_tempBufferViews[(size_t)write.pTexelBufferView - 1];
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
        Key(static_cast<vk::DescriptorSetLayoutCreateInfo>(copy)) {
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

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayoutCreateFlags& flags):
        m_device(device),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder(const vk::DescriptorSetLayoutCreateFlags& flags):
        m_device(Engine::graphics()->getDevice()),
        m_flags(flags) {
}

DescriptorSetLayoutBuilder::~DescriptorSetLayoutBuilder() = default;

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addUniformBuffer(uint32_t binding, vk::ShaderStageFlags shaderStages, bool dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout UniformBlock binding %d - The binding is already added", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageBuffer(uint32_t binding, vk::ShaderStageFlags shaderStages, bool dynamic) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout StorageBlock binding %d - The binding is already added", binding);
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


DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageTexelBuffer(uint32_t binding, vk::ShaderStageFlags shaderStages) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout StorageTexelBuffer binding %d - The binding is already added", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampler(uint32_t binding, vk::ShaderStageFlags shaderStages, uint32_t arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout Sampler binding %d - The binding is already added", binding);
        assert(false);
    }
    if (arraySize == 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout Sampler binding %d - Array size must not be zero", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addSampledImage(uint32_t binding, vk::ShaderStageFlags shaderStages, uint32_t arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout SampledImage binding %d - The binding is already added", binding);
        assert(false);
    }
    if (arraySize == 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout SampledImage binding %d - Array size must not be zero", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addCombinedImageSampler(uint32_t binding, vk::ShaderStageFlags shaderStages, uint32_t arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout CombinedImageSampler binding %d - The binding is already added", binding);
        assert(false);
    }
    if (arraySize == 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout CombinedImageSampler binding %d - Array size must not be zero", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addInputAttachment(uint32_t binding, vk::ShaderStageFlags shaderStages, uint32_t arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout InputAttachment binding %d - The binding is already added", binding);
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addStorageImage(uint32_t binding, vk::ShaderStageFlags shaderStages, uint32_t arraySize) {
#if _DEBUG
    if (m_bindings.count(binding) != 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout StorageImage binding %d - The binding is already added", binding);
        assert(false);
    }
    if (arraySize == 0) {
        LOG_FATAL("Unable to add DescriptorSetLayout StorageImage binding %d - Array size must not be zero", binding);
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

SharedResource<DescriptorSetLayout> DescriptorSetLayoutBuilder::build(const std::string& name) {
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

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::reset(const vk::DescriptorSetLayoutCreateFlags& flags) {
    m_flags = flags;
    m_bindings.clear();
    return *this;
}
