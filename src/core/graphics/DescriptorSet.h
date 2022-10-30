
#ifndef WORLDENGINE_DESCRIPTORSET_H
#define WORLDENGINE_DESCRIPTORSET_H

#include "core/core.h"

class Buffer;
class BufferView;
class Sampler;
class ImageView;
class Texture;

class DescriptorSet;
class DescriptorPool;

struct DescriptorPoolConfiguration {
    WeakResource<vkr::Device> device;
    vk::DescriptorPoolCreateFlags flags;
    uint32_t maxSets = 1000;

    std::unordered_map<vk::DescriptorType, uint32_t> poolSizes = {
            { vk::DescriptorType::eSampler, 500 },
            { vk::DescriptorType::eCombinedImageSampler, 4000 },
            { vk::DescriptorType::eSampledImage, 4000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 2000 },
            { vk::DescriptorType::eStorageBuffer, 2000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 500 }
    };
};


class DescriptorSetLayout {
    NO_COPY(DescriptorSetLayout);

public:
    struct Key : public vk::DescriptorSetLayoutCreateInfo {
        explicit Key(const vk::DescriptorSetLayoutCreateInfo& rhs);

        Key(const Key& copy);

        Key(Key&& move) noexcept ;

        ~Key();

        bool operator==(const Key& rhs) const;
    };

    struct KeyHasher {
        size_t operator()(const Key& descriptorSetLayoutKey) const;
    };

    typedef std::unordered_map<Key, WeakResource<DescriptorSetLayout>, KeyHasher> Cache;

private:
    DescriptorSetLayout(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayout& descriptorSetLayout, Key key, const std::string& name);

public:
    ~DescriptorSetLayout();

    static SharedResource<DescriptorSetLayout> get(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const std::string& name);

    static void clearCache();

    DescriptorSet* createDescriptorSet(const SharedResource<DescriptorPool>& descriptorPool, const std::string& name);

    bool createDescriptorSets(const SharedResource<DescriptorPool>& descriptorPool, const uint32_t& count, DescriptorSet** outDescriptorSets, const std::string& name);

    bool createDescriptorSets(const SharedResource<DescriptorPool>& descriptorPool, const uint32_t& count, SharedResource<DescriptorSet>* outDescriptorSets, const std::string& name);

    const SharedResource<vkr::Device>& getDevice() const;

    const vk::DescriptorSetLayout& getDescriptorSetLayout() const;

    std::vector<vk::DescriptorSetLayoutBinding> getBindings() const;

    bool hasBinding(const uint32_t& binding) const;

    int32_t findBindingIndex(const uint32_t& binding) const;

    const vk::DescriptorSetLayoutBinding& findBinding(const uint32_t& binding) const;

    const vk::DescriptorSetLayoutBinding& getBinding(const int32_t& index) const;

    const uint32_t& getBindingCount() const;

    bool operator==(const DescriptorSetLayout& rhs) const;

    bool operator!=(const DescriptorSetLayout& rhs) const;

    const GraphicsResource& getResourceId() const;

private:
    SharedResource<vkr::Device> m_device;
    vk::DescriptorSetLayout m_descriptorSetLayout;
    Key m_key;
    std::string m_name;

    GraphicsResource m_resourceId;

    static Cache s_descriptorSetLayoutCache;
};



class DescriptorSetLayoutBuilder {
    NO_COPY(DescriptorSetLayoutBuilder);
public:
    explicit DescriptorSetLayoutBuilder(const WeakResource<vkr::Device>& device, const vk::DescriptorSetLayoutCreateFlags& flags = {});

    explicit DescriptorSetLayoutBuilder(const vk::DescriptorSetLayoutCreateFlags& flags = {});

    ~DescriptorSetLayoutBuilder();

    DescriptorSetLayoutBuilder& addUniformBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const bool& dynamic = false);

    DescriptorSetLayoutBuilder& addStorageBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const bool& dynamic = false);

    DescriptorSetLayoutBuilder& addStorageTexelBuffer(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages);

    DescriptorSetLayoutBuilder& addSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize = 1);

    DescriptorSetLayoutBuilder& addSampledImage(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize = 1);

    DescriptorSetLayoutBuilder& addCombinedImageSampler(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize = 1);

    DescriptorSetLayoutBuilder& addInputAttachment(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize = 1);

    DescriptorSetLayoutBuilder& addStorageImage(const uint32_t& binding, const vk::ShaderStageFlags& shaderStages, const uint32_t& arraySize = 1);

    SharedResource<DescriptorSetLayout> build(const std::string& name);

    DescriptorSetLayoutBuilder& reset(const vk::DescriptorSetLayoutCreateFlags& flags = {});

private:
    WeakResource<vkr::Device> m_device;
    vk::DescriptorSetLayoutCreateFlags m_flags;
    std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> m_bindings;
};


class DescriptorPool {
    NO_COPY(DescriptorPool);
private:
    DescriptorPool(const WeakResource<vkr::Device>& device, vk::DescriptorPool descriptorPool, bool canFreeDescriptorSets, const std::string& name);

public:
    ~DescriptorPool();

    static DescriptorPool* create(const DescriptorPoolConfiguration& descriptorPoolConfiguration, const std::string& name);

    const SharedResource<vkr::Device>& getDevice() const;

    const vk::DescriptorPool& getDescriptorPool() const;

    bool allocate(const vk::DescriptorSetLayout& descriptorSetLayout, vk::DescriptorSet& outDescriptorSet) const;

    void free(const vk::DescriptorSet& descriptorSet);

    const bool& canFreeDescriptorSets() const;

    const GraphicsResource& getResourceId() const;

private:
    SharedResource<vkr::Device> m_device;
    vk::DescriptorPool m_descriptorPool;
    bool m_canFreeDescriptorSets;

    GraphicsResource m_resourceId;
};



class DescriptorSet {
    NO_COPY(DescriptorSet);
private:
    DescriptorSet(const WeakResource<vkr::Device>& device, const WeakResource<DescriptorPool>& pool, const WeakResource<DescriptorSetLayout>& layout, const vk::DescriptorSet& descriptorSet, const std::string& name);

public:
    DescriptorSet(DescriptorSet&& move) noexcept;

    ~DescriptorSet();

    static DescriptorSet* create(const vk::DescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo, const WeakResource<DescriptorPool>& descriptorPool, const std::string& name, const std::string& layoutName);

    static DescriptorSet* create(const WeakResource<DescriptorSetLayout>& descriptorSetLayout, const WeakResource<DescriptorPool>& descriptorPool, const std::string& name);

    const vk::DescriptorSet& getDescriptorSet() const;

    const SharedResource<vkr::Device>& getDevice() const;

    const SharedResource<DescriptorPool>& getPool() const;

    const SharedResource<DescriptorSetLayout>& getLayout() const;

    const GraphicsResource& getResourceId() const;

private:
    SharedResource<vkr::Device> m_device;
    SharedResource<DescriptorPool> m_pool;
    SharedResource<DescriptorSetLayout> m_layout;
    vk::DescriptorSet m_descriptorSet;
    GraphicsResource m_resourceId;
};


class DescriptorSetWriter {
    NO_COPY(DescriptorSetWriter);
public:
    explicit DescriptorSetWriter(DescriptorSet* descriptorSet);

    explicit DescriptorSetWriter(const SharedResource<DescriptorSet>& descriptorSet);

    explicit DescriptorSetWriter(const WeakResource<DescriptorSet>& descriptorSet);

    ~DescriptorSetWriter();

    DescriptorSetWriter& writeBuffer(const uint32_t& binding, const vk::DescriptorBufferInfo& bufferInfo);
    DescriptorSetWriter& writeBuffer(const uint32_t& binding, const vk::Buffer& buffer, const vk::DeviceSize& offset = 0, const vk::DeviceSize& range = VK_WHOLE_SIZE);
    DescriptorSetWriter& writeBuffer(const uint32_t& binding, const Buffer* buffer, const vk::DeviceSize& offset = 0, const vk::DeviceSize& range = VK_WHOLE_SIZE);

    DescriptorSetWriter& writeTexelBufferView(const uint32_t& binding, const vk::BufferView& bufferView);
    DescriptorSetWriter& writeTexelBufferView(const uint32_t& binding, const BufferView* bufferView);

    DescriptorSetWriter& writeImage(const uint32_t& binding, const vk::DescriptorImageInfo* imageInfos, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const vk::DescriptorImageInfo& imageInfo, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const vk::Sampler* samplers, const vk::ImageView* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const vk::Sampler& sampler, const vk::ImageView& imageView, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Sampler* const* samplers, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* const* imageViews, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Sampler* sampler, const ImageView* imageView, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Texture* const* textures, const vk::ImageLayout* imageLayouts, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Texture* const* textures, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount);
    DescriptorSetWriter& writeImage(const uint32_t& binding, const Texture* texture, const vk::ImageLayout& imageLayout, const uint32_t& arrayIndex, const uint32_t& arrayCount);

    bool write();

private:
    std::vector<vk::WriteDescriptorSet> m_writes;
    DescriptorSet* m_descriptorSet;

    std::vector<vk::BufferView> m_tempBufferViews;
    std::vector<vk::DescriptorBufferInfo> m_tempBufferInfo;
    std::vector<vk::DescriptorImageInfo> m_tempImageInfo;
};

#endif //WORLDENGINE_DESCRIPTORSET_H
