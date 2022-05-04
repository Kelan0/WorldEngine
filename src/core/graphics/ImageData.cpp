#include "core/graphics/ImageData.h"
#include "core/application/Application.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/CommandPool.h"

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
        return nullptr;
    }

    if (data == nullptr) {
        printf("Failed to load image \"%s\"\n", filePath.c_str());
        return nullptr;
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
        return nullptr;
    }

    ImagePixelFormat format =
            channelSize == 1 ? ImagePixelFormat::UInt8 :
            channelSize == 2 ? ImagePixelFormat::UInt16 :
            channelSize == 4 ? ImagePixelFormat::Float32 :
            ImagePixelFormat::Invalid;

    if (format == ImagePixelFormat::Invalid) {
        printf("Failed to load image \"%s\": Invalid pixel format for %d bytes per channel\n", filePath.c_str(), channelSize);
        stbi_image_free(data);
        return nullptr;
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
        return nullptr;
    }

    if (srcFormat == ImagePixelFormat::Invalid || dstFormat == ImagePixelFormat::Invalid) {
        printf("Unable to mutate image pixels: Invalid pixel format\n");
        return nullptr;
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

ImageData* ImageData::transform(const ImageData* imageData, const ImageTransform& transformation) {
    return transform(imageData->getData(), imageData->getWidth(), imageData->getHeight(), imageData->getPixelLayout(), imageData->getPixelFormat(), transformation);
}

ImageData* ImageData::transform(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format, const ImageTransform& transformation) {
    return transformation.apply(data, width, height, layout, format);
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


ImageData* ImageData::ImageTransform::apply(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format) const {
    // No-op implementation - The image just gets copied.

    size_t pixelStride = ImageData::getChannels(layout) * ImageData::getChannelSize(format);
    assert(pixelStride != 0);

    uint8_t* dstPixels = static_cast<uint8_t*>(malloc(height * width * pixelStride));
    memcpy(dstPixels, data, height * width * pixelStride);

    return new ImageData(dstPixels, width, height, layout, format, false);
}

bool ImageData::ImageTransform::isNoOp() const {
    return true;
}

ImageData::Flip::Flip(bool x, bool y):
    flip_x(x),
    flip_y(y) {
}

ImageData::Flip::Flip(const Flip& copy):
    flip_x(copy.flip_x),
    flip_y(copy.flip_y) {
}

ImageData* ImageData::Flip::apply(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format) const {
    // TODO: use compute shader for this

    if (isNoOp())
        return ImageTransform::apply(data, width, height, layout, format);

    if (layout == ImagePixelLayout::Invalid) {
        printf("Unable to transform image pixels: Invalid pixel layout\n");
        return nullptr;
    }

    if (format == ImagePixelFormat::Invalid) {
        printf("Unable to transform image pixels: Invalid pixel format\n");
        return nullptr;
    }

    size_t pixelStride = ImageData::getChannels(layout) * ImageData::getChannelSize(format);
    assert(pixelStride != 0);
    size_t rowStride = pixelStride * width;
    size_t numPixels = height * width;
    size_t numBytes = numPixels * pixelStride;
    uint8_t* srcPixels = data;
    uint8_t* dstPixels = static_cast<uint8_t*>(malloc(numBytes));

    uint8_t *src, *dst;

    for (size_t y = 0; y < height; ++y) {
        src = &srcPixels[y * rowStride];

        if (flip_y)
            dst = &dstPixels[(height - y - 1) * rowStride];
        else
            dst = &dstPixels[y * rowStride];

        if (flip_x) {
            dst = &dst[(width - 1) * pixelStride];
            for (size_t x = 0; x < width; ++x, src += pixelStride, dst -= pixelStride)
                memcpy(dst, src, pixelStride);
        } else {
            // X-axis is not flipped, we can copy a whole row to the destination
            memcpy(dst, src, rowStride);
        }
    }

    return new ImageData(dstPixels, width, height, layout, format, false);
}

bool ImageData::Flip::isNoOp() const {
    return !flip_x && !flip_y;
}



bool ImageUtil::isDepthAttachment(const vk::Format& format) {
    switch (format) {
        case vk::Format::eD16Unorm:
        case vk::Format::eX8D24UnormPack32:
        case vk::Format::eD32Sfloat:
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return true;
        default:
            return false;
    }
}

bool ImageUtil::isStencilAttachment(const vk::Format& format) {
    switch (format) {
        case vk::Format::eS8Uint:
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return true;
        default:
            return false;
    }
}

vk::Format ImageUtil::selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
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

bool ImageUtil::getImageFormatProperties(const vk::Format& format, const vk::ImageType& type, const vk::ImageTiling& tiling, const vk::ImageUsageFlags& usage, const vk::ImageCreateFlags& flags, vk::ImageFormatProperties* imageFormatProperties) {
    const vk::PhysicalDevice& physicalDevice = Application::instance()->graphics()->getPhysicalDevice();

    vk::Result result;
    result = physicalDevice.getImageFormatProperties(format, type, tiling, usage, flags, imageFormatProperties);

    if (result != vk::Result::eSuccess) {
        if (result == vk::Result::eErrorFormatNotSupported)
            printf("Unable to get image format properties: requested image format %s is not supported by the physical device\n", vk::to_string(format).c_str());
        else
            printf("Unable to get image format properties: \n", vk::to_string(result).c_str());
        return false;
    }

    return true;
}


bool ImageUtil::validateImageCreateInfo(const vk::ImageCreateInfo& imageCreateInfo) {
    vk::ImageFormatProperties imageFormatProperties;
    if (!getImageFormatProperties(imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling, imageCreateInfo.usage, imageCreateInfo.flags, &imageFormatProperties)) {
        return false;
    }

    if (imageCreateInfo.extent.width > imageFormatProperties.maxExtent.width ||
        imageCreateInfo.extent.height > imageFormatProperties.maxExtent.height ||
        imageCreateInfo.extent.depth > imageFormatProperties.maxExtent.depth) {
        printf("Unable to create Image: requested image extent [%d x %d x %d] is greater than the maximum supported extent for this format [%d x %d x %d]\n",
               imageCreateInfo.extent.width, imageCreateInfo.extent.height, imageCreateInfo.extent.depth,
               imageFormatProperties.maxExtent.width, imageFormatProperties.maxExtent.height, imageFormatProperties.maxExtent.depth);
        return false;
    }

    if (imageCreateInfo.mipLevels > imageFormatProperties.maxMipLevels) {
        printf("Unable to create Image: %d requested mip levels is greater than the maximum %d mip levels supported for this format\n",
               imageCreateInfo.mipLevels, imageFormatProperties.maxMipLevels);
        return false;
    }

    if (imageCreateInfo.arrayLayers > imageFormatProperties.maxArrayLayers) {
        printf("Unable to create Image: %d requested array layers is greater than the maximum %d array layers supported for this format\n",
               imageCreateInfo.arrayLayers, imageFormatProperties.maxArrayLayers);
        return false;
    }

    // TODO: validate sampleCount
    // TODO: validate maxResourceSize

    return true;
}

bool ImageUtil::transitionLayout(const vk::Image& image, vk::CommandBuffer commandBuffer, const vk::ImageSubresourceRange& subresourceRange, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    vk::ImageMemoryBarrier barrier;
    barrier.setImage(image);
    barrier.setOldLayout(srcState.layout);
    barrier.setNewLayout(dstState.layout);
    barrier.setSrcAccessMask(srcState.accessMask);
    barrier.setDstAccessMask(dstState.accessMask);
    barrier.setSrcQueueFamilyIndex(srcState.queueFamilyIndex);
    barrier.setDstQueueFamilyIndex(dstState.queueFamilyIndex);
    barrier.setSubresourceRange(subresourceRange);

    vk::PipelineStageFlags srcStageFlags = srcState.pipelineStage;
    vk::PipelineStageFlags dstStageFlags = dstState.pipelineStage;

    commandBuffer.pipelineBarrier(srcStageFlags, dstStageFlags, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    return true;
}

bool ImageUtil::upload(const vk::Image& dstImage, void* data, const uint32_t& bytesPerPixel, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {

    if (!dstImage) {
        assert(false);
        return false;
    }

    if (data == nullptr) {
        assert(false);
        return false;
    }

    if (bytesPerPixel == 0) {
        assert(false);
        return false;
    }

    if (imageRegion.width == VK_WHOLE_SIZE
        || imageRegion.height == VK_WHOLE_SIZE
        || imageRegion.depth == VK_WHOLE_SIZE
        || imageRegion.layerCount == VK_WHOLE_SIZE
        || imageRegion.mipLevelCount == VK_WHOLE_SIZE) {
        printf("Unable to upload Image: Image region is out of bounds\n");
        return false;
    }

    vk::BufferImageCopy imageCopy;
    imageCopy.setBufferOffset(0);
    imageCopy.setBufferRowLength(0);
    imageCopy.setBufferImageHeight(0);
    imageCopy.imageSubresource.setAspectMask(aspectMask);
    imageCopy.imageSubresource.setMipLevel(imageRegion.baseMipLevel);
    imageCopy.imageSubresource.setBaseArrayLayer(imageRegion.baseLayer);
    imageCopy.imageSubresource.setLayerCount(imageRegion.layerCount);
    imageCopy.imageOffset.setX(imageRegion.x);
    imageCopy.imageOffset.setY(imageRegion.y);
    imageCopy.imageOffset.setZ(imageRegion.z);
    imageCopy.imageExtent.setWidth(imageRegion.width);
    imageCopy.imageExtent.setHeight(imageRegion.height);
    imageCopy.imageExtent.setDepth(imageRegion.depth);

    BufferConfiguration bufferConfig;
    bufferConfig.device = Application::instance()->graphics()->getDevice();
    bufferConfig.data = data;
    bufferConfig.size = (vk::DeviceSize)imageRegion.width * (vk::DeviceSize)imageRegion.height * (vk::DeviceSize)imageRegion.depth * (vk::DeviceSize)bytesPerPixel;
    bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    bufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
    Buffer* srcBuffer = Buffer::create(bufferConfig);
    if (srcBuffer == nullptr) {
        printf("Failed to create image data staging buffer\n");
        return false;
    }

    const vk::Queue& transferQueue = **Application::instance()->graphics()->getQueue(QUEUE_TRANSFER_MAIN);
    const vk::CommandBuffer& transferCommandBuffer = **Application::instance()->graphics()->commandPool()->getCommandBuffer("transfer_buffer");

    vk::CommandBufferBeginInfo commandBeginInfo;
    commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(aspectMask);
    subresourceRange.setBaseArrayLayer(imageRegion.baseLayer);
    subresourceRange.setLayerCount(imageRegion.layerCount);
    subresourceRange.setBaseMipLevel(imageRegion.baseMipLevel);
    subresourceRange.setLevelCount(imageRegion.mipLevelCount);
    transferCommandBuffer.begin(commandBeginInfo);
    ImageUtil::transitionLayout(dstImage, transferCommandBuffer, subresourceRange, ImageTransition::FromAny(), ImageTransition::TransferDst());
    transferCommandBuffer.copyBufferToImage(srcBuffer->getBuffer(), dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageCopy);
    ImageUtil::transitionLayout(dstImage, transferCommandBuffer, subresourceRange, ImageTransition::TransferDst(), dstState);
    transferCommandBuffer.end();

    vk::SubmitInfo queueSumbitInfo;
    queueSumbitInfo.setCommandBufferCount(1);
    queueSumbitInfo.setPCommandBuffers(&transferCommandBuffer);
    transferQueue.submit(1, &queueSumbitInfo, VK_NULL_HANDLE);
    transferQueue.waitIdle();

    delete srcBuffer;

    return true;
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