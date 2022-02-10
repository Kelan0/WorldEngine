#pragma once

#include "../core.h"

enum class ImageChannels {
	Gray = 1,
	GrayAlpha = 2,
	RGB = 3,
	RGBA = 4
};

class ImageData {
	NO_COPY(ImageData);
private:
	ImageData(uint8_t* data, uint32_t width, uint32_t height, uint32_t channels);

public:
	~ImageData();

	static std::shared_ptr<ImageData> load(const std::string& filePath, ImageChannels desiredChannels);

	static void unload(const std::string& filePath);

	static void clearCache();

private:
	uint8_t* m_data;
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_channels;

	static std::map<std::string, std::shared_ptr<ImageData>> s_imageCache;
};

class Image {
	NO_COPY(Image);
private:
	Image(std::shared_ptr<vkr::Device> device, vk::Image image, vk::DeviceMemory deviceMemory);

public:
	~Image();

private:
	std::shared_ptr<vkr::Device> m_device;
	vk::Image m_image;
	vk::DeviceMemory m_deviceMemory;

};