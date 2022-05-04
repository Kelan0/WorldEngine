
#ifndef WORLDENGINE_TEXTURE_H
#define WORLDENGINE_TEXTURE_H


#include "core/core.h"
#include "core/graphics/Image2D.h"

struct SamplerConfiguration {
    std::weak_ptr<vkr::Device> device;
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
    Sampler(std::weak_ptr<vkr::Device> device, vk::Sampler sampler);

public:
    ~Sampler();

    static Sampler* create(const SamplerConfiguration& samplerConfiguration);

    static std::shared_ptr<Sampler> get(const SamplerConfiguration& samplerConfiguration);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Sampler& getSampler();

private:
    struct Key : public SamplerConfiguration {
        Key(const SamplerConfiguration& rhs);

        bool operator==(const Key& rhs) const;
    };

    struct KeyHasher {
        size_t operator()(const Key& samplerKey) const;
    };
private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Sampler m_sampler;

    static std::unordered_map<Key, std::weak_ptr<Sampler>, KeyHasher> s_cachedSamplers;
};

class Texture2D {
public:
    Texture2D(std::weak_ptr<ImageView2D> imageView, std::weak_ptr<Sampler> sampler);

    ~Texture2D();

    static Texture2D* create(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler);

    static Texture2D* create(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration);

    static Texture2D* create(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler);

    static Texture2D* create(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration);

    std::shared_ptr<ImageView2D> getImageView() const;

    std::shared_ptr<Sampler> getSampler() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<ImageView2D> m_imageView;
    std::shared_ptr<Sampler> m_sampler;
    GraphicsResource m_resourceId;
};



#endif //WORLDENGINE_TEXTURE_H
