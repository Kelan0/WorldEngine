
#ifndef WORLDENGINE_TEXTURE_H
#define WORLDENGINE_TEXTURE_H


#include "core/core.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"

struct SamplerConfiguration {
    WeakResource<vkr::Device> device;
    vk::Filter magFilter = vk::Filter::eLinear;
    vk::Filter minFilter = vk::Filter::eLinear;
    vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
    vk::SamplerAddressMode wrapU = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode wrapV = vk::SamplerAddressMode::eRepeat;
    vk::SamplerAddressMode wrapW = vk::SamplerAddressMode::eRepeat;
    float mipLodBias = 0.0F;
    bool enableAnisotropy = false;
    float maxAnisotropy = 0.0F;
    bool enableCompare = false;
    vk::CompareOp compareOp = vk::CompareOp::eNever;
    float minLod = 0.0F;
    float maxLod = 0.0F;
    vk::BorderColor borderColour = vk::BorderColor::eFloatTransparentBlack;
    bool unnormalizedCoordinates = false;
};

class Sampler {
    NO_COPY(Sampler)
private:
    Sampler(const WeakResource<vkr::Device>& device, const vk::Sampler& sampler, const std::string& name);

public:
    ~Sampler();

    static Sampler* create(const SamplerConfiguration& samplerConfiguration, const std::string& name);

    static std::shared_ptr<Sampler> get(const SamplerConfiguration& samplerConfiguration, const std::string& name);

    const SharedResource<vkr::Device>& getDevice() const;

    const vk::Sampler& getSampler() const;

private:
    struct Key : public SamplerConfiguration {
        Key(const SamplerConfiguration& rhs);

        bool operator==(const Key& rhs) const;
    };

    struct KeyHasher {
        size_t operator()(const Key& samplerKey) const;
    };
private:
    SharedResource<vkr::Device> m_device;
    vk::Sampler m_sampler;

    static std::unordered_map<Key, std::weak_ptr<Sampler>, KeyHasher> s_cachedSamplers;
};

class Texture {
public:
    Texture(const std::weak_ptr<ImageView>& imageView, const std::weak_ptr<Sampler>& sampler);

    ~Texture();

    static Texture* create(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const std::string& name);

    static Texture* create(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const std::string& name);

    static Texture* create(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const std::string& name);

    static Texture* create(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const std::string& name);

    const vk::ImageViewType& getType() const;

    std::shared_ptr<ImageView> getImageView() const;

    std::shared_ptr<Sampler> getSampler() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<ImageView> m_imageView;
    std::shared_ptr<Sampler> m_sampler;
    GraphicsResource m_resourceId;
};



#endif //WORLDENGINE_TEXTURE_H
