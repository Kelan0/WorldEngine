#include "Texture.h"
#include "GraphicsManager.h"

std::unordered_map<Sampler::Key, std::weak_ptr<Sampler>, Sampler::KeyHasher> Sampler::s_cachedSamplers;

Sampler::Sampler(std::weak_ptr<vkr::Device> device, vk::Sampler sampler):
	m_device(device),
	m_sampler(sampler) {
	//printf("Create sampler\n");
}

Sampler::~Sampler() {
	//printf("Destroying sampler\n");
	(**m_device).destroySampler(m_sampler);
}

Sampler* Sampler::create(const SamplerConfiguration& samplerConfiguration) {

	const vk::Device& device = **samplerConfiguration.device.lock();

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

	vk::Sampler sampler = VK_NULL_HANDLE;

	vk::Result result;
	result = device.createSampler(&samplerCreateInfo, NULL, &sampler);
	if (result != vk::Result::eSuccess) {
		printf("Failed to create sampler: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	return new Sampler(samplerConfiguration.device, sampler);
}

std::shared_ptr<Sampler> Sampler::get(const SamplerConfiguration& samplerConfiguration) {
	Key key(samplerConfiguration);
	auto it = s_cachedSamplers.find(key);

	if (it == s_cachedSamplers.end() || it->second.expired()) {
		std::shared_ptr<Sampler> sampler(Sampler::create(samplerConfiguration));
		if (sampler == NULL)
			return NULL;
		s_cachedSamplers.insert(std::make_pair(key, std::weak_ptr<Sampler>(sampler)));
		return sampler;
	} else {
		return std::shared_ptr<Sampler>(it->second);
	}
}

std::shared_ptr<vkr::Device> Sampler::getDevice() const {
	return m_device;
}

const vk::Sampler& Sampler::getSampler() {
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
	if ((VkDevice)(**device.lock()) != (VkDevice)(**rhs.device.lock())) return false;
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
	std::hash_combine(s, (VkDevice)(**samplerKey.device.lock()));
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

Texture2D::Texture2D(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler):
	m_imageView(std::move(image)),
	m_sampler(std::move(sampler)),
	m_resourceId(GraphicsManager::nextResourceId()) {
}

Texture2D::~Texture2D() {
	m_sampler.reset();
	m_imageView.reset();
}

Texture2D* Texture2D::create(std::weak_ptr<ImageView2D> image, std::weak_ptr<Sampler> sampler) {
	return new Texture2D(std::move(image), std::move(sampler));
}

Texture2D* Texture2D::create(std::weak_ptr<ImageView2D> image, const SamplerConfiguration& samplerConfiguration) {
	std::shared_ptr<Sampler> sampler = Sampler::get(samplerConfiguration);
	if (sampler == NULL)
		return NULL;

	return new Texture2D(std::move(image), std::move(sampler));
}

Texture2D* Texture2D::create(const ImageView2DConfiguration& imageView2DConfiguration, std::weak_ptr<Sampler> sampler) {
	ImageView2D* rawImageView = ImageView2D::create(imageView2DConfiguration);
	if (rawImageView == NULL)
		return NULL;

	return new Texture2D(std::shared_ptr<ImageView2D>(rawImageView), std::move(sampler));
}

Texture2D* Texture2D::create(const ImageView2DConfiguration& imageView2DConfiguration, const SamplerConfiguration& samplerConfiguration) {
	ImageView2D* rawImageView = ImageView2D::create(imageView2DConfiguration);
	if (rawImageView == NULL)
		return NULL;
	std::shared_ptr<ImageView2D> imageView = std::shared_ptr<ImageView2D>(rawImageView);
	
	std::shared_ptr<Sampler> sampler = Sampler::get(samplerConfiguration);
	if (sampler == NULL) {
		return NULL;
	}
	
	return new Texture2D(std::move(imageView), std::move(sampler));
}

std::shared_ptr<ImageView2D> Texture2D::getImageView() const {
	return m_imageView;
}

std::shared_ptr<Sampler> Texture2D::getSampler() const {
	return m_sampler;
}

const GraphicsResource& Texture2D::getResourceId() const {
	return m_resourceId;
}
