#include "Image.h"


std::map<std::string, std::shared_ptr<ImageData>> ImageData::s_imageCache;

ImageData::ImageData(uint8_t* data, uint32_t width, uint32_t height, uint32_t channels):
	m_data(data),
	m_width(width),
	m_height(height),
	m_channels(channels) {

}

ImageData::~ImageData() {
	stbi_image_free(m_data);
}

std::shared_ptr<ImageData> ImageData::load(const std::string& filePath, ImageChannels desiredChannels) {
	auto it = s_imageCache.find(filePath);
	if (it != s_imageCache.end()) {
		return it->second;
	}

	int width, height, channels;
	stbi_uc* data = stbi_load(filePath.c_str(), &width, &height, &channels, (int)desiredChannels);
	if (data == NULL) {
		printf("Failed to load image \"%s\"\n", filePath.c_str());
		return NULL;
	}

	printf("Loaded load image \"%s\"\n", filePath.c_str());

	std::shared_ptr<ImageData> image(new ImageData(reinterpret_cast<uint8_t*>(data), width, height, channels));
	
	s_imageCache.insert(std::make_pair(filePath, image));

	return image;
}

void ImageData::unload(const std::string& filePath) {
	printf("Unloading image \"%s\"\n", filePath);
	s_imageCache.erase(filePath);
}

void ImageData::clearCache() {
	s_imageCache.clear();
}





Image::Image(std::shared_ptr<vkr::Device> device, vk::Image image, vk::DeviceMemory deviceMemory):
	m_device(std::move(device)),
	m_image(image),
	m_deviceMemory(deviceMemory) {
}

Image::~Image() {
	(**m_device).destroyImage(m_image);
	(**m_device).freeMemory(m_deviceMemory);
}