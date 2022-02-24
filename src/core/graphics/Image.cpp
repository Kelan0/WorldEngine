#include "Image.h"
#include "Buffer.h"
#include "GPUMemory.h"
#include "CommandPool.h"
#include "GraphicsManager.h"
#include "../application/Application.h"


std::map<std::string, ImageData*> ImageData::s_imageCache;

ImageData::ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, bool stbiAllocated):
	m_data(data),
	m_width(width),
	m_height(height),
	m_pixelLayout(pixelLayout),
	m_pixelFormat(pixelFormat),
	m_stbiAllocated(stbiAllocated),
	m_externallyAllocated(false) {
}

ImageData::ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat) :
	m_data(data),
	m_width(width),
	m_height(height),
	m_pixelLayout(pixelLayout),
	m_pixelFormat(pixelFormat),
	m_stbiAllocated(false),
	m_externallyAllocated(true) {
}

ImageData::~ImageData() {
	if (m_stbiAllocated) {
		stbi_image_free(m_data);

	} else if (!m_externallyAllocated) {
		free(m_data);
	}
}

ImageData* ImageData::load(const std::string& filePath, ImagePixelLayout desiredLayout, ImagePixelFormat desiredFormat) {
	auto it = s_imageCache.find(filePath);
	if (it != s_imageCache.end()) {
		return it->second;
	}

	int channelSize, desiredChannelCount;

	if (desiredFormat != ImagePixelFormat::Invalid) {
		channelSize = ImageData::getChannelSize(desiredFormat);
	} else {
		channelSize = 1;
	}

	if (desiredLayout != ImagePixelLayout::Invalid) {
		desiredChannelCount = ImageData::getChannels(desiredLayout);
	} else {
		desiredChannelCount = 0; // Let stb choose
	}

	int width, height, channels;
	uint8_t* data;

	if (channelSize == 1) {
		data = reinterpret_cast<uint8_t*>(stbi_load(filePath.c_str(), &width, &height, &channels, desiredChannelCount));
	} else if (channelSize == 2) {
		data = reinterpret_cast<uint8_t*>(stbi_load_16(filePath.c_str(), &width, &height, &channels, desiredChannelCount));
	} else if (channelSize == 4) {
		data = reinterpret_cast<uint8_t*>(stbi_loadf(filePath.c_str(), &width, &height, &channels, desiredChannelCount));
	} else {
		printf("Unable to load image \"%s\": Invalid image data format\n", filePath.c_str());
		return NULL;
	}

	if (data == NULL) {
		printf("Failed to load image \"%s\"\n", filePath.c_str());
		return NULL;
	}

	if (desiredChannelCount != 0) {
		channels = desiredChannelCount; // We forced this many channels to load.
	}

	ImagePixelLayout layout = 
		channels == 1 ? ImagePixelLayout::R : 
		channels == 2 ? ImagePixelLayout::RG : 
		channels == 3 ? ImagePixelLayout::RGB : 
		channels == 4 ? ImagePixelLayout::RGBA : 
		ImagePixelLayout::Invalid;

	if (layout == ImagePixelLayout::Invalid) {
		printf("Failed to load image \"%s\": Invalid pixel layout for %d channels\n", filePath.c_str(), channels);
		stbi_image_free(data);
		return NULL;
	}

	ImagePixelFormat format =
		channelSize == 1 ? ImagePixelFormat::UInt8 :
		channelSize == 2 ? ImagePixelFormat::UInt16 :
		channelSize == 4 ? ImagePixelFormat::Float32 :
		ImagePixelFormat::Invalid;

	if (format == ImagePixelFormat::Invalid) {
		printf("Failed to load image \"%s\": Invalid pixel format for %d bytes per channel\n", filePath.c_str(), channelSize);
		stbi_image_free(data);
		return NULL;
	}

	printf("Loaded load image \"%s\"\n", filePath.c_str());

	ImageData* image = new ImageData(data, width, height, layout, format, true);
	
	s_imageCache.insert(std::make_pair(filePath, image));

	return image;
}

void ImageData::unload(const std::string& filePath) {
	printf("Unloading image \"%s\"\n", filePath.c_str());
	auto it = s_imageCache.find(filePath);
	if (it != s_imageCache.end()) {
		delete it->second;
		s_imageCache.erase(it);
	}
}

void ImageData::clearCache() {
	s_imageCache.clear();
}

ImageData* ImageData::mutate(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat) {
	uint8_t* mutatedPixels;

	if (srcLayout == ImagePixelLayout::Invalid || dstLayout == ImagePixelLayout::Invalid) {
		printf("Unable to mutate image pixels: Invalid pixel layout\n");
		return NULL;
	}

	if (srcFormat == ImagePixelFormat::Invalid || dstFormat == ImagePixelFormat::Invalid) {
		printf("Unable to mutate image pixels: Invalid pixel format\n");
		return NULL;
	}

	if (srcLayout == dstLayout && srcFormat == dstFormat) {
		size_t size = (size_t)width * (size_t)height * ImageData::getChannels(srcLayout) * ImageData::getChannelSize(srcFormat);
		mutatedPixels = static_cast<uint8_t*>(malloc(size));
		memcpy(mutatedPixels, data, size);

	} else {

		int srcChannels = ImageData::getChannels(srcLayout);
		int srcStride = srcChannels * ImageData::getChannelSize(srcFormat);
		assert(srcStride != 0);

		int dstChannels = ImageData::getChannels(dstLayout);
		int dstStride = dstChannels * ImageData::getChannelSize(dstFormat);
		assert(dstStride != 0);

		mutatedPixels = static_cast<uint8_t*>(malloc((size_t)width * (size_t)height * dstStride));

		uint8_t* srcPixel = data;
		uint8_t* dstPixel = mutatedPixels;

		uint32_t zero, one;

		if (dstFormat == ImagePixelFormat::Float16 || dstFormat == ImagePixelFormat::Float32) {
			float fzero = 0.0F, fone = 1.0F;
			zero = *reinterpret_cast<uint32_t*>(&fzero);
			one = *reinterpret_cast<uint32_t*>(&fone);
		} else {
			zero = 0, one = 0xFFFFFFFF;
		}


		glm::uvec4 channels(zero, zero, zero, one);

		//vk::ComponentSwizzle srcSwizzle[4], dstSwizzle[4];
		//ImageData::getPixelSwizzle(srcLayout, srcSwizzle);
		//ImageData::getPixelSwizzle(dstLayout, dstSwizzle);

		int swizzleTransform[4] = { 0, 1, 2, 3 };


		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				for (int i = 0; i < srcChannels; ++i) {
					switch (srcFormat) {
					case ImagePixelFormat::UInt8: channels[swizzleTransform[i]] = reinterpret_cast<uint8_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::SInt8: channels[swizzleTransform[i]] = reinterpret_cast<int8_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::UInt16: channels[swizzleTransform[i]] = reinterpret_cast<uint16_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::SInt16: channels[swizzleTransform[i]] = reinterpret_cast<int16_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::UInt32: channels[swizzleTransform[i]] = reinterpret_cast<uint32_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::SInt32: channels[swizzleTransform[i]] = reinterpret_cast<int32_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::Float16: channels[swizzleTransform[i]] = reinterpret_cast<uint16_t*>(srcPixel)[i]; break;
					case ImagePixelFormat::Float32: channels[swizzleTransform[i]] = reinterpret_cast<uint32_t*>(srcPixel)[i]; break;
					}
				}
				for (int i = 0; i < dstChannels; ++i) {
					switch (dstFormat) {
					case ImagePixelFormat::UInt8: reinterpret_cast<uint8_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::SInt8: reinterpret_cast<int8_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::UInt16: reinterpret_cast<uint16_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::SInt16: reinterpret_cast<int16_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::UInt32: reinterpret_cast<uint32_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::SInt32: reinterpret_cast<int32_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::Float16: reinterpret_cast<uint16_t*>(dstPixel)[i] = channels[i]; break;
					case ImagePixelFormat::Float32: reinterpret_cast<uint32_t*>(dstPixel)[i] = channels[i]; break;
					}
				}

				srcPixel += srcStride;
				dstPixel += dstStride;
			}
		}
	}

	return new ImageData(mutatedPixels, width, height, dstLayout, dstFormat, false);
}

uint8_t* ImageData::getData() const {
	return m_data;
}

uint32_t ImageData::getWidth() const {
	return m_width;
}

uint32_t ImageData::getHeight() const {
	return m_height;
}

ImagePixelLayout ImageData::getPixelLayout() const {
	return m_pixelLayout;
}

ImagePixelFormat ImageData::getPixelFormat() const {
	return m_pixelFormat;
}

int ImageData::getChannels(ImagePixelLayout layout) {
	switch (layout) {
	case ImagePixelLayout::R:
		return 1;
	case ImagePixelLayout::RG:
		return 2;
	case ImagePixelLayout::RGB:
	case ImagePixelLayout::BGR:
		return 3;
	case ImagePixelLayout::RGBA:
	case ImagePixelLayout::ABGR:
		return 4;
	}
	return 0;
}

int ImageData::getChannelSize(ImagePixelFormat format) {
	switch (format) {
	case ImagePixelFormat::UInt8:
	case ImagePixelFormat::SInt8:
		return 1;
	case ImagePixelFormat::UInt16:
	case ImagePixelFormat::SInt16:
	case ImagePixelFormat::Float16:
		return 2;
	case ImagePixelFormat::UInt32:
	case ImagePixelFormat::SInt32:
	case ImagePixelFormat::Float32:
		return 4;
	}
	return 0;
}

bool ImageData::getPixelSwizzle(ImagePixelLayout layout, vk::ComponentSwizzle swizzle[4]) {
	switch (layout) {
	case ImagePixelLayout::R:
		swizzle[0] = vk::ComponentSwizzle::eR;
		swizzle[1] = vk::ComponentSwizzle::eR;
		swizzle[2] = vk::ComponentSwizzle::eR;
		swizzle[3] = vk::ComponentSwizzle::eOne;
		return true;
	case ImagePixelLayout::RG:
		swizzle[0] = vk::ComponentSwizzle::eR;
		swizzle[1] = vk::ComponentSwizzle::eG;
		swizzle[2] = vk::ComponentSwizzle::eZero;
		swizzle[3] = vk::ComponentSwizzle::eOne;
		return true;
	case ImagePixelLayout::RGB:
		swizzle[0] = vk::ComponentSwizzle::eR;
		swizzle[1] = vk::ComponentSwizzle::eG;
		swizzle[2] = vk::ComponentSwizzle::eB;
		swizzle[3] = vk::ComponentSwizzle::eOne;
		return true;
	case ImagePixelLayout::BGR:
		swizzle[0] = vk::ComponentSwizzle::eB;
		swizzle[1] = vk::ComponentSwizzle::eG;
		swizzle[2] = vk::ComponentSwizzle::eR;
		swizzle[3] = vk::ComponentSwizzle::eOne;
		return true;
	case ImagePixelLayout::RGBA:
		swizzle[0] = vk::ComponentSwizzle::eR;
		swizzle[1] = vk::ComponentSwizzle::eG;
		swizzle[2] = vk::ComponentSwizzle::eB;
		swizzle[2] = vk::ComponentSwizzle::eA;
		return true;
	case ImagePixelLayout::ABGR:
		swizzle[0] = vk::ComponentSwizzle::eA;
		swizzle[1] = vk::ComponentSwizzle::eB;
		swizzle[2] = vk::ComponentSwizzle::eG;
		swizzle[2] = vk::ComponentSwizzle::eR;
		return true;
	default:
		return false;
	}
}

bool ImageData::getPixelLayoutAndFormat(vk::Format format, ImagePixelLayout& outLayout, ImagePixelFormat& outFormat) {
	switch (format) {
		// RGBA
	case vk::Format::eR8G8B8A8Uscaled:
	case vk::Format::eR8G8B8A8Unorm:
	case vk::Format::eR8G8B8A8Uint:
	case vk::Format::eR8G8B8A8Srgb:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::UInt8;
		return true;
	case vk::Format::eR8G8B8A8Sscaled:
	case vk::Format::eR8G8B8A8Snorm:
	case vk::Format::eR8G8B8A8Sint:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::SInt8;
		return true;
	case vk::Format::eR16G16B16A16Uscaled:
	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Uint:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::UInt16;
		return true;
	case vk::Format::eR16G16B16A16Sscaled:
	case vk::Format::eR16G16B16A16Snorm:
	case vk::Format::eR16G16B16A16Sint:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::SInt16;
		return true;
	case vk::Format::eR16G16B16A16Sfloat:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::Float16;
		return true;
	case vk::Format::eR32G32B32A32Uint:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::UInt32;
		return true;
	case vk::Format::eR32G32B32A32Sint:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::SInt32;
		return true;
	case vk::Format::eR32G32B32A32Sfloat:
		outLayout = ImagePixelLayout::RGBA;
		outFormat = ImagePixelFormat::Float32;
		return true;

		// RGB
	case vk::Format::eR8G8B8Uscaled:
	case vk::Format::eR8G8B8Unorm:
	case vk::Format::eR8G8B8Uint:
	case vk::Format::eR8G8B8Srgb:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::UInt8;
		return true;
	case vk::Format::eR8G8B8Sscaled:
	case vk::Format::eR8G8B8Snorm:
	case vk::Format::eR8G8B8Sint:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::SInt8;
		return true;
	case vk::Format::eR16G16B16Uscaled:
	case vk::Format::eR16G16B16Unorm:
	case vk::Format::eR16G16B16Uint:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::UInt16;
		return true;
	case vk::Format::eR16G16B16Sscaled:
	case vk::Format::eR16G16B16Snorm:
	case vk::Format::eR16G16B16Sint:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::SInt16;
		return true;
	case vk::Format::eR16G16B16Sfloat:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::Float16;
		return true;
	case vk::Format::eR32G32B32Uint:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::UInt32;
		return true;
	case vk::Format::eR32G32B32Sint:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::SInt32;
		return true;
	case vk::Format::eR32G32B32Sfloat:
		outLayout = ImagePixelLayout::RGB;
		outFormat = ImagePixelFormat::Float32;
		return true;

		// BGR
	case vk::Format::eB8G8R8Uscaled:
	case vk::Format::eB8G8R8Unorm:
	case vk::Format::eB8G8R8Uint:
	case vk::Format::eB8G8R8Srgb:
		outLayout = ImagePixelLayout::BGR;
		outFormat = ImagePixelFormat::UInt8;
		return true;
	case vk::Format::eB8G8R8Sscaled:
	case vk::Format::eB8G8R8Snorm:
	case vk::Format::eB8G8R8Sint:
		outLayout = ImagePixelLayout::BGR;
		outFormat = ImagePixelFormat::SInt8;
		return true;

		// RG
	case vk::Format::eR8G8Uscaled:
	case vk::Format::eR8G8Unorm:
	case vk::Format::eR8G8Uint:
	case vk::Format::eR8G8Srgb:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::UInt8;
		return true;
	case vk::Format::eR8G8Sscaled:
	case vk::Format::eR8G8Snorm:
	case vk::Format::eR8G8Sint:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::SInt8;
		return true;
	case vk::Format::eR16G16Uscaled:
	case vk::Format::eR16G16Unorm:
	case vk::Format::eR16G16Uint:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::UInt16;
		return true;
	case vk::Format::eR16G16Sscaled:
	case vk::Format::eR16G16Snorm:
	case vk::Format::eR16G16Sint:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::SInt16;
		return true;
	case vk::Format::eR16G16Sfloat:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::Float16;
		return true;
	case vk::Format::eR32G32Uint:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::UInt32;
		return true;
	case vk::Format::eR32G32Sint:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::SInt32;
		return true;
	case vk::Format::eR32G32Sfloat:
		outLayout = ImagePixelLayout::RG;
		outFormat = ImagePixelFormat::Float32;
		return true;

		// R
	case vk::Format::eR8Uscaled:
	case vk::Format::eR8Unorm:
	case vk::Format::eR8Uint:
	case vk::Format::eR8Srgb:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::UInt8;
		return true;
	case vk::Format::eR8Sscaled:
	case vk::Format::eR8Snorm:
	case vk::Format::eR8Sint:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::SInt8;
		return true;
	case vk::Format::eR16Uscaled:
	case vk::Format::eR16Unorm:
	case vk::Format::eR16Uint:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::UInt16;
		return true;
	case vk::Format::eR16Sscaled:
	case vk::Format::eR16Snorm:
	case vk::Format::eR16Sint:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::SInt16;
		return true;
	case vk::Format::eR16Sfloat:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::Float16;
		return true;
	case vk::Format::eR32Uint:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::UInt32;
		return true;
	case vk::Format::eR32Sint:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::SInt32;
		return true;
	case vk::Format::eR32Sfloat:
		outLayout = ImagePixelLayout::R;
		outFormat = ImagePixelFormat::Float32;
		return true;
	default:
		outLayout = ImagePixelLayout::Invalid;
		outFormat = ImagePixelFormat::Invalid;
		return false;
	}
}






Image2D::Image2D(std::weak_ptr<vkr::Device> device, vk::Image image, vk::DeviceMemory deviceMemory, uint32_t width, uint32_t height, vk::Format format):
	m_device(device),
	m_image(image),
	m_deviceMemory(deviceMemory),
	m_width(width),
	m_height(height),
	m_format(format) {
	//printf("Create image\n");
}

Image2D::~Image2D() {
	//printf("Destroying image\n");
	(**m_device).destroyImage(m_image);
	(**m_device).freeMemory(m_deviceMemory);
}

Image2D* Image2D::create(const Image2DConfiguration& image2DConfiguration) {
	const vk::Device& device = **image2DConfiguration.device.lock();

	vk::Result result;

	uint32_t width = 0;
	uint32_t height = 0;

	ImageData* imageData = image2DConfiguration.imageData;
	if (imageData == NULL) {
		if (!image2DConfiguration.filePath.empty()) {
			// This image data will stay loaded
			ImagePixelLayout pixelLayout;
			ImagePixelFormat pixelFormat;
			if (!ImageData::getPixelLayoutAndFormat(image2DConfiguration.format, pixelLayout, pixelFormat)) {
				printf("Unable to create image: supplied image format %s has no corresponding loadable pixel format or layout\n", vk::to_string(image2DConfiguration.format).c_str());
				return NULL;
			}
			imageData = ImageData::load(image2DConfiguration.filePath, pixelLayout, pixelFormat);
		}
	}

	if (imageData != NULL) {
		width = imageData->getWidth();
		height = imageData->getHeight();
	} else {
		width = image2DConfiguration.width;
		height = image2DConfiguration.height;
	}

	vk::ImageUsageFlags usage = image2DConfiguration.usage;
	if (imageData != NULL) {
		usage |= vk::ImageUsageFlagBits::eTransferDst;
	}

	vk::ImageCreateInfo imageCreateInfo;
	imageCreateInfo.setFlags({});
	imageCreateInfo.setImageType(vk::ImageType::e2D);
	imageCreateInfo.setFormat(image2DConfiguration.format);
	imageCreateInfo.extent.setWidth(width);
	imageCreateInfo.extent.setHeight(height);
	imageCreateInfo.extent.setDepth(1);
	imageCreateInfo.setMipLevels(image2DConfiguration.mipLevels);
	imageCreateInfo.setArrayLayers(1);
	imageCreateInfo.setSamples(image2DConfiguration.sampleCount);
	imageCreateInfo.setTiling(image2DConfiguration.enabledTexelAccess ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal);
	imageCreateInfo.setUsage(usage);
	imageCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
	imageCreateInfo.setInitialLayout(image2DConfiguration.preInitialized ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined);

	vk::ImageFormatProperties imageFormatProperties;
	const vk::PhysicalDevice& physicalDevice = Application::instance()->graphics()->getPhysicalDevice();

	result = physicalDevice.getImageFormatProperties(imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling, imageCreateInfo.usage, imageCreateInfo.flags, &imageFormatProperties);

	if (result != vk::Result::eSuccess) {
		if (result == vk::Result::eErrorFormatNotSupported)
			printf("Unable to create image: requested image format %s is not supported by the physical device\n", vk::to_string(imageCreateInfo.format).c_str());
		else
			printf("Unable to create image: \n", vk::to_string(result).c_str());
		return NULL;
	}

	if (imageCreateInfo.extent.width > imageFormatProperties.maxExtent.width ||
		imageCreateInfo.extent.height > imageFormatProperties.maxExtent.height ||
		imageCreateInfo.extent.depth > imageFormatProperties.maxExtent.depth) {
		printf("Unable to create image: requested image extent [%d x %d x %d] is greater than the maximum supported extent for this format [%d x %d x %d]\n",
			imageCreateInfo.extent.width, imageCreateInfo.extent.height, imageCreateInfo.extent.depth, 
			imageFormatProperties.maxExtent.width, imageFormatProperties.maxExtent.height, imageFormatProperties.maxExtent.depth);
		return NULL;
	}

	if (imageCreateInfo.mipLevels > imageFormatProperties.maxMipLevels) {
		printf("Unable to create image: %d requested mip levels is greated than the maximum %d mip levels supported for this format\n", 
			imageCreateInfo.mipLevels, imageFormatProperties.maxMipLevels);
		return NULL;
	}

	if (imageCreateInfo.arrayLayers > imageFormatProperties.maxArrayLayers) {
		printf("Unable to create image: %d requested array layers is greated than the maximum %d array layers supported for this format\n", 
			imageCreateInfo.arrayLayers, imageFormatProperties.maxArrayLayers);
		return NULL;
	}

	// TODO: validate sampleCount
	// TODO: validate maxResourceSize

	vk::Image image = VK_NULL_HANDLE;
	result = device.createImage(&imageCreateInfo, NULL, &image);
	if (result != vk::Result::eSuccess) {
		printf("Failed to create image: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
	
	uint32_t memoryTypeIndex;
	if (!GPUMemory::selectMemoryType(memoryRequirements.memoryTypeBits, image2DConfiguration.memoryProperties, memoryTypeIndex)) {
		device.destroyImage(image);
		printf("Image memory allocation failed\n");
		return NULL;
	}

	vk::MemoryAllocateInfo allocateInfo;
	allocateInfo.setAllocationSize(memoryRequirements.size);
	allocateInfo.setMemoryTypeIndex(memoryTypeIndex);

	vk::DeviceMemory deviceMemory = VK_NULL_HANDLE;
	result = device.allocateMemory(&allocateInfo, NULL, &deviceMemory);

	if (result != vk::Result::eSuccess) {
		device.destroyImage(image);
		printf("Failed to allocate device memory for image: %s\n", vk::to_string(result));
		return NULL;
	}

	device.bindImageMemory(image, deviceMemory, 0);

	Image2D* returnImage = new Image2D(image2DConfiguration.device, image, deviceMemory, width, height, imageCreateInfo.format);

	if (imageData != NULL) {
		ImageRegion uploadRegion;
		uploadRegion.width = imageData->getWidth();
		uploadRegion.height = imageData->getHeight();
		ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
		if (!returnImage->upload(imageData->getData(), imageData->getPixelLayout(), imageData->getPixelFormat(), vk::ImageAspectFlagBits::eColor, uploadRegion, dstState)) {
			printf("Failed to upload buffer data\n");
			delete returnImage;
			return NULL;
		}
	}

	return returnImage;
}

bool Image2D::upload(Image2D* dstImage, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {

	assert(dstImage != NULL);
	assert(data != NULL);

	if (pixelLayout == ImagePixelLayout::Invalid) {
		printf("Unable to upload image data: Invalid image pixel layout\n");
		return false;
	}

	if (pixelFormat == ImagePixelFormat::Invalid) {
		printf("Unable to upload image data: Invalid image pixel format\n");
		return false;
	}

	ImagePixelLayout dstPixelLayout;
	ImagePixelFormat dstPixelFormat;
	if (!ImageData::getPixelLayoutAndFormat(dstImage->getFormat(), dstPixelLayout, dstPixelFormat)) {
		printf("Unable to upload image data: There is no corresponding pixel layout or format for the destination images format %s\n", vk::to_string(dstImage->getFormat()).c_str());
		return false;
	}

	if (!validateImageRegion(dstImage, imageRegion)) {
		return false;
	}

	void* uploadData = data;
	ImageData* tempImageData = NULL;

	if (dstPixelFormat != pixelFormat || dstPixelLayout != pixelLayout) {
		tempImageData = ImageData::mutate(static_cast<uint8_t*>(data), imageRegion.width, imageRegion.height, pixelLayout, pixelFormat, dstPixelLayout, dstPixelFormat);
		uploadData = tempImageData->getData();
	}

	int bytesPerPixel = ImageData::getChannelSize(dstPixelFormat) * ImageData::getChannels(dstPixelLayout);

	if (bytesPerPixel == 0) {
		printf("Unable to upload image data: Invalid image pixel format\n");
		return false;
	}

	vk::BufferImageCopy imageCopy;
	imageCopy.setBufferOffset(0);
	imageCopy.setBufferRowLength(0);
	imageCopy.setBufferImageHeight(0);
	imageCopy.imageSubresource.setAspectMask(aspectMask);
	imageCopy.imageSubresource.setMipLevel(imageRegion.baseMipLevel);
	imageCopy.imageSubresource.setBaseArrayLayer(0);
	imageCopy.imageSubresource.setLayerCount(1);
	imageCopy.imageOffset.setX(imageRegion.x);
	imageCopy.imageOffset.setY(imageRegion.y);
	imageCopy.imageOffset.setZ(0);
	imageCopy.imageExtent.setWidth(imageRegion.width);
	imageCopy.imageExtent.setHeight(imageRegion.height);
	imageCopy.imageExtent.setDepth(1);

	BufferConfiguration bufferConfig;
	bufferConfig.device = dstImage->getDevice();
	bufferConfig.data = uploadData;
	bufferConfig.size = (vk::DeviceSize)imageRegion.width * (vk::DeviceSize)imageRegion.height * (vk::DeviceSize)bytesPerPixel;
	bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	bufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
	Buffer* srcBuffer = Buffer::create(bufferConfig);

	if (tempImageData != NULL)
		delete tempImageData;

	const vk::Queue& transferQueue = **Application::instance()->graphics()->getQueue(QUEUE_TRANSFER_MAIN);
	const vk::CommandBuffer& transferCommandBuffer = **Application::instance()->graphics()->commandPool()->getCommandBuffer("transfer_buffer");

	vk::CommandBufferBeginInfo commandBeginInfo;
	commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	transferCommandBuffer.begin(commandBeginInfo);
	Image2D::transitionLayout(dstImage, transferCommandBuffer, aspectMask, ImageTransition::FromAny(), ImageTransition::TransferDst());
	transferCommandBuffer.copyBufferToImage(srcBuffer->getBuffer(), dstImage->getImage(), vk::ImageLayout::eTransferDstOptimal, 1, &imageCopy);
	Image2D::transitionLayout(dstImage, transferCommandBuffer, aspectMask, ImageTransition::TransferDst(), dstState);
	transferCommandBuffer.end();

	vk::SubmitInfo queueSumbitInfo;
	queueSumbitInfo.setCommandBufferCount(1);
	queueSumbitInfo.setPCommandBuffers(&transferCommandBuffer);
	transferQueue.submit(1, &queueSumbitInfo, VK_NULL_HANDLE);
	transferQueue.waitIdle();

	delete srcBuffer;

	return true;
}

bool Image2D::transitionLayout(Image2D* image, vk::CommandBuffer commandBuffer, vk::ImageAspectFlags aspectMask, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {

	vk::ImageMemoryBarrier barrier;
	barrier.setImage(image->getImage());
	barrier.setOldLayout(srcState.layout);
	barrier.setNewLayout(dstState.layout);
	barrier.setSrcAccessMask(srcState.accessMask);
	barrier.setDstAccessMask(dstState.accessMask);
	barrier.setSrcQueueFamilyIndex(srcState.queueFamilyIndex);
	barrier.setDstQueueFamilyIndex(dstState.queueFamilyIndex);
	barrier.subresourceRange.setAspectMask(aspectMask);
	barrier.subresourceRange.setBaseMipLevel(0);
	barrier.subresourceRange.setLevelCount(1);
	barrier.subresourceRange.setBaseArrayLayer(0);
	barrier.subresourceRange.setLayerCount(1);

	vk::PipelineStageFlags srcStageFlags = srcState.pipelineStage;
	vk::PipelineStageFlags dstStageFlags = dstState.pipelineStage;

	commandBuffer.pipelineBarrier(srcStageFlags, dstStageFlags, {}, 0, NULL, 0, NULL, 1, &barrier);

	return true;
}

bool Image2D::upload(void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
	return Image2D::upload(this, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

std::shared_ptr<vkr::Device> Image2D::getDevice() const {
	return m_device;
}

const vk::Image& Image2D::getImage() const {
	return m_image;
}

uint32_t Image2D::getWidth() const {
	return m_width;
}

uint32_t Image2D::getHeight() const {
	return m_height;
}

vk::Format Image2D::getFormat() const {
	return m_format;
}

vk::Format Image2D::selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
	for (int i = 0; i < candidates.size(); ++i) {
		const vk::Format& format = candidates[i];
		vk::FormatProperties props = physicalDevice.getFormatProperties(format);

		if ((tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) || 
			(tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)) {

			return format;
		}
	}

	return vk::Format::eUndefined;
}

bool Image2D::validateImageRegion(Image2D* image, ImageRegion& imageRegion) {
	if (image == NULL) {
		return false;
	}

	if (imageRegion.x >= image->getWidth() || imageRegion.y >= image->getHeight()) {
		printf("Image region out of range\n");
		return false;
	}

	if (imageRegion.width == VK_WHOLE_SIZE) imageRegion.width = image->getWidth() - imageRegion.x;
	if (imageRegion.height == VK_WHOLE_SIZE) imageRegion.height = image->getHeight() - imageRegion.y;

	if (imageRegion.x + imageRegion.width > image->getWidth() || imageRegion.y + imageRegion.height > image->getHeight()) {
		printf("Image region out of range\n");
		return false;
	}

	return true;
}


ImageView2D::ImageView2D(std::weak_ptr<vkr::Device> device, vk::ImageView imageView):
	m_device(device),
	m_imageView(imageView) {
	//printf("Create ImageView\n");
}

ImageView2D::~ImageView2D() {
	//printf("Destroy ImageView\n");
	(**m_device).destroyImageView(m_imageView);
}

ImageView2D* ImageView2D::create(const ImageView2DConfiguration& imageView2DConfiguration) {
	const vk::Device& device = **imageView2DConfiguration.device.lock();

	if (!imageView2DConfiguration.image) {
		printf("Unable to create 2D image view: Image is NULL\n");
		return NULL;
	}

	vk::ImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.setImage(imageView2DConfiguration.image);
	imageViewCreateInfo.setViewType(vk::ImageViewType::e2D);
	imageViewCreateInfo.setFormat(imageView2DConfiguration.format);
	imageViewCreateInfo.components.setR(imageView2DConfiguration.redSwizzle);
	imageViewCreateInfo.components.setG(imageView2DConfiguration.greenSwizzle);
	imageViewCreateInfo.components.setB(imageView2DConfiguration.blueSwizzle);
	imageViewCreateInfo.components.setA(imageView2DConfiguration.alphaSwizzle);
	imageViewCreateInfo.subresourceRange.setAspectMask(imageView2DConfiguration.aspectMask);
	imageViewCreateInfo.subresourceRange.setBaseMipLevel(imageView2DConfiguration.baseMipLevel);
	imageViewCreateInfo.subresourceRange.setLevelCount(imageView2DConfiguration.mipLevelCount);
	imageViewCreateInfo.subresourceRange.setBaseArrayLayer(imageView2DConfiguration.baseArrayLayer);
	imageViewCreateInfo.subresourceRange.setLayerCount(imageView2DConfiguration.arrayLayerCount);

	vk::ImageView imageView = VK_NULL_HANDLE;

	vk::Result result;
	result = device.createImageView(&imageViewCreateInfo, NULL, &imageView);

	if (result != vk::Result::eSuccess) {
		printf("Failed to create image view: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	return new ImageView2D(imageView2DConfiguration.device, imageView);
}

std::shared_ptr<vkr::Device> ImageView2D::getDevice() const {
	return m_device;
}

const vk::ImageView& ImageView2D::getImageView() const {
	return m_imageView;
}





ImageTransition::FromAny::FromAny() {
	layout = vk::ImageLayout::eUndefined;
	pipelineStage = vk::PipelineStageFlagBits::eTopOfPipe;
	accessMask = vk::Flags<vk::AccessFlagBits>(0);
}

ImageTransition::TransferDst::TransferDst() {
	layout = vk::ImageLayout::eTransferDstOptimal;
	pipelineStage = vk::PipelineStageFlagBits::eTransfer;
	accessMask = vk::AccessFlagBits::eTransferWrite;
}

ImageTransition::TransferSrc::TransferSrc() {
	layout = vk::ImageLayout::eTransferSrcOptimal;
	pipelineStage = vk::PipelineStageFlagBits::eTopOfPipe;
	accessMask = vk::AccessFlagBits::eTransferRead;
}

ImageTransition::ShaderAccess::ShaderAccess(vk::PipelineStageFlags shaderPipelineStages, bool read, bool write) {
#if _DEBUG
	constexpr vk::PipelineStageFlags validShaderStages = 
		vk::PipelineStageFlagBits::eVertexShader | 
		vk::PipelineStageFlagBits::eGeometryShader | 
		vk::PipelineStageFlagBits::eTessellationControlShader | 
		vk::PipelineStageFlagBits::eTessellationEvaluationShader | 
		vk::PipelineStageFlagBits::eFragmentShader | 
		vk::PipelineStageFlagBits::eComputeShader | 
		vk::PipelineStageFlagBits::eRayTracingShaderNV | 
		vk::PipelineStageFlagBits::eTaskShaderNV;
		vk::PipelineStageFlagBits::eMeshShaderNV;
		if ((shaderPipelineStages & (~validShaderStages))) {
			printf("Provided pipeline stages for ShaderAccess image transition must only contain shader stages\n");
			assert(false);
		}
		if (!read && !write) {
			printf("Provided access flags for ShaderAccess image transition must be readable, writable or both\n");
			assert(false);
		}
#endif
	pipelineStage = shaderPipelineStages;
	layout = write ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
	if (read) accessMask |= vk::AccessFlagBits::eShaderRead;
	if (write) accessMask |= vk::AccessFlagBits::eShaderWrite;
}

ImageTransition::ShaderReadOnly::ShaderReadOnly(vk::PipelineStageFlags shaderPipelineStages) :
	ShaderAccess(shaderPipelineStages, true, false) {
}

ImageTransition::ShaderWriteOnly::ShaderWriteOnly(vk::PipelineStageFlags shaderPipelineStages) :
	ShaderAccess(shaderPipelineStages, false, true) {
}

ImageTransition::ShaderReadWrite::ShaderReadWrite(vk::PipelineStageFlags shaderPipelineStages) :
	ShaderAccess(shaderPipelineStages, true, true) {
}
