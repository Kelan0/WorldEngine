#include "core/graphics/Texture.h"
#include "core/graphics/GraphicsManager.h"
#include "core/util/Logger.h"

std::unordered_map<Sampler::Key, std::weak_ptr<Sampler>, Sampler::KeyHasher> Sampler::s_cachedSamplers;

Sampler::Sampler(const WeakResource<vkr::Device>& device, const vk::Sampler& sampler, const std::string& name):
        GraphicsResource(ResourceType_Sampler, device, name),
        m_sampler(sampler) {
    //printf("Create sampler\n");
}

Sampler::~Sampler() {
    //printf("Destroying sampler\n");
    (**m_device).destroySampler(m_sampler);
}

Sampler* Sampler::create(const SamplerConfiguration& samplerConfiguration, const std::string& name) {

    const vk::Device& device = **samplerConfiguration.device.lock(name);

    vk::SamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.setMagFilter(samplerConfiguration.magFilter);
    samplerCreateInfo.setMinFilter(samplerConfiguration.minFilter);
    samplerCreateInfo.setMipmapMode(samplerConfiguration.mipmapMode);
    samplerCreateInfo.setAddressModeU(samplerConfiguration.wrapU);
    samplerCreateInfo.setAddressModeV(samplerConfiguration.wrapV);
    samplerCreateInfo.setAddressModeW(samplerConfiguration.wrapW);
    samplerCreateInfo.setMipLodBias(samplerConfiguration.mipLodBias);
    samplerCreateInfo.setAnisotropyEnable(samplerConfiguration.enableAnisotropy);
    samplerCreateInfo.setMaxAnisotropy(samplerConfiguration.maxAnisotropy);
    samplerCreateInfo.setCompareEnable(samplerConfiguration.enableCompare);
    samplerCreateInfo.setCompareOp(samplerConfiguration.compareOp);
    samplerCreateInfo.setMinLod(samplerConfiguration.minLod);
    samplerCreateInfo.setMaxLod(samplerConfiguration.maxLod);
    samplerCreateInfo.setBorderColor(samplerConfiguration.borderColour);
    samplerCreateInfo.setUnnormalizedCoordinates(samplerConfiguration.unnormalizedCoordinates);

    vk::Sampler sampler = nullptr;

    vk::Result result;
    result = device.createSampler(&samplerCreateInfo, nullptr, &sampler);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create sampler: %s", vk::to_string(result).c_str());
        return nullptr;
    }
    Engine::graphics()->setObjectName(device, (uint64_t)(VkSampler)sampler, vk::ObjectType::eSampler, name);


    return new Sampler(samplerConfiguration.device, sampler, name);
}

std::shared_ptr<Sampler> Sampler::get(const SamplerConfiguration& samplerConfiguration, const std::string& name) {
    Key key(samplerConfiguration);
    auto it = s_cachedSamplers.find(key);

    if (it == s_cachedSamplers.end() || it->second.expired()) {
        std::shared_ptr<Sampler> sampler(Sampler::create(samplerConfiguration, name));
        if (sampler == nullptr)
            return nullptr;
        s_cachedSamplers.insert(std::make_pair(key, std::weak_ptr<Sampler>(sampler)));
        return sampler;
    } else {
        return std::shared_ptr<Sampler>(it->second);
    }
}

const vk::Sampler& Sampler::getSampler() const {
    return m_sampler;
}

Sampler::Key::Key(const SamplerConfiguration& rhs) {
    device = rhs.device;
    magFilter = rhs.magFilter;
    minFilter = rhs.minFilter;
    mipmapMode = rhs.mipmapMode;
    wrapU = rhs.wrapU;
    wrapV = rhs.wrapV;
    wrapW = rhs.wrapW;
    mipLodBias = rhs.mipLodBias;
    enableAnisotropy = rhs.enableAnisotropy;
    maxAnisotropy = rhs.maxAnisotropy;
    enableCompare = rhs.enableCompare;
    compareOp = rhs.compareOp;
    minLod = rhs.minLod;
    maxLod = rhs.maxLod;
    borderColour = rhs.borderColour;
    unnormalizedCoordinates = rhs.unnormalizedCoordinates;
}

#define F2I(x) ((uint64_t)((double)x * 100000))

bool Sampler::Key::operator==(const Key& rhs) const {
    if ((VkDevice)(**device.lock("Sampler::Key")) != (VkDevice)(**rhs.device.lock("Sampler::Key"))) return false;
    if (magFilter != rhs.magFilter) return false;
    if (minFilter != rhs.minFilter) return false;
    if (mipmapMode != rhs.mipmapMode) return false;
    if (wrapU != rhs.wrapU) return false;
    if (wrapV != rhs.wrapV) return false;
    if (wrapW != rhs.wrapW) return false;
    if (F2I(mipLodBias) != F2I(rhs.mipLodBias)) return false;
    if (enableAnisotropy != rhs.enableAnisotropy) return false;
    if (F2I(maxAnisotropy) != F2I(rhs.maxAnisotropy)) return false;
    if (enableCompare != rhs.enableCompare) return false;
    if (compareOp != rhs.compareOp) return false;
    if (F2I(minLod) != F2I(rhs.minLod)) return false;
    if (F2I(maxLod) != F2I(rhs.maxLod)) return false;
    if (borderColour != rhs.borderColour) return false;
    if (unnormalizedCoordinates != rhs.unnormalizedCoordinates) return false;
    return true;
}

size_t Sampler::KeyHasher::operator()(const Key& samplerKey) const {
    size_t s = 0;
    std::hash_combine(s, (VkDevice)(**samplerKey.device.lock("Sampler::KeyHasher")));
    std::hash_combine(s, samplerKey.magFilter);
    std::hash_combine(s, samplerKey.minFilter);
    std::hash_combine(s, samplerKey.mipmapMode);
    std::hash_combine(s, samplerKey.wrapU);
    std::hash_combine(s, samplerKey.wrapV);
    std::hash_combine(s, samplerKey.wrapW);
    std::hash_combine(s, F2I(samplerKey.mipLodBias));
    std::hash_combine(s, samplerKey.enableAnisotropy);
    std::hash_combine(s, F2I(samplerKey.maxAnisotropy));
    std::hash_combine(s, samplerKey.enableCompare);
    std::hash_combine(s, samplerKey.compareOp);
    std::hash_combine(s, F2I(samplerKey.minLod));
    std::hash_combine(s, F2I(samplerKey.maxLod));
    std::hash_combine(s, samplerKey.borderColour);
    std::hash_combine(s, samplerKey.unnormalizedCoordinates);
    return s;
}

Texture::Texture(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const std::string& name):
        GraphicsResource(ResourceType_Texture, image.lock()->getDevice(), name),
        m_imageView(image),
        m_sampler(sampler) {
    assert(m_imageView->getDevice() == m_sampler->getDevice());
}

Texture::~Texture() {
    m_sampler.reset();
    m_imageView.reset();
}

Texture* Texture::create(const std::weak_ptr<ImageView>& image, const std::weak_ptr<Sampler>& sampler, const std::string& name) {
    return new Texture(image, sampler, name);
}

Texture* Texture::create(const std::weak_ptr<ImageView>& image, const SamplerConfiguration& samplerConfiguration, const std::string& name) {
    std::shared_ptr<Sampler> sampler = Sampler::get(samplerConfiguration, name);
    if (sampler == nullptr)
        return nullptr;

    return new Texture(image, sampler, name);
}

Texture* Texture::create(const ImageViewConfiguration& imageViewConfiguration, const std::weak_ptr<Sampler>& sampler, const std::string& name) {
    ImageView* rawImageView = ImageView::create(imageViewConfiguration, name);
    if (rawImageView == nullptr)
        return nullptr;

    return new Texture(std::shared_ptr<ImageView>(rawImageView), sampler, name);
}

Texture* Texture::create(const ImageViewConfiguration& imageViewConfiguration, const SamplerConfiguration& samplerConfiguration, const std::string& name) {
    ImageView* rawImageView = ImageView::create(imageViewConfiguration, name);
    if (rawImageView == nullptr)
        return nullptr;
    std::shared_ptr<ImageView> imageView = std::shared_ptr<ImageView>(rawImageView);

    std::shared_ptr<Sampler> sampler = Sampler::get(samplerConfiguration, name);
    if (sampler == nullptr) {
        return nullptr;
    }

    return new Texture(imageView, sampler, name);
}

const vk::ImageViewType& Texture::getType() const {
    return m_imageView->getType();
}

std::shared_ptr<ImageView> Texture::getImageView() const {
    return m_imageView;
}

std::shared_ptr<Sampler> Texture::getSampler() const {
    return m_sampler;
}
