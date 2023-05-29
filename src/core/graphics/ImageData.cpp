#include "core/graphics/ImageData.h"
#include "core/application/Engine.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/util/Util.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/application/Application.h"

std::unordered_map<std::string, ImageData*> ImageData::s_imageCache;
std::unordered_map<std::string, ComputePipeline*> ImageData::ImageTransform::s_transformComputePipelines;

Buffer* g_imageStagingBuffer = nullptr;

ImageData::ImageData(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, AllocationType allocationType):
        m_data(data),
        m_width(width),
        m_height(height),
        m_pixelLayout(pixelLayout),
        m_pixelFormat(pixelFormat),
        m_allocationType(allocationType) {
}

//ImageData::ImageData(ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat):
//        ImageData(nullptr, width, height, pixelLayout, pixelFormat, AllocationType_Internal) {
//        size_t allocSize = (size_t)width * (size_t)height * (size_t)ImageData::getChannels(pixelLayout) * (size_t)ImageData::getChannelSize(pixelFormat);
//        m_data = static_cast<uint8_t*>(malloc(allocSize));
//}

ImageData::ImageData(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat):
        ImageData(data, width, height, pixelLayout, pixelFormat, AllocationType_External) {
}

ImageData::ImageData(ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat):
        ImageData(nullptr, width, height, pixelLayout, pixelFormat, AllocationType_Internal) {

    size_t pixelSize = ImageData::getChannels(pixelLayout) * ImageData::getChannelSize(pixelFormat);
    m_data = static_cast<uint8_t*>(malloc(width * height * pixelSize));
}

ImageData::~ImageData() {
    if (m_allocationType == AllocationType_Stbi) {
        stbi_image_free(m_data);
    } else if (m_allocationType == AllocationType_Internal) {
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

    std::string absFilePath = Application::instance()->getAbsoluteResourceFilePath(filePath);

    LOG_INFO("Loading image \"%s\"", absFilePath.c_str());
    auto t0 = Performance::now();

    if (channelSize == 1) {
        data = reinterpret_cast<uint8_t*>(stbi_load(absFilePath.c_str(), &width, &height, &channels, desiredChannelCount));
    } else if (channelSize == 2) {
        data = reinterpret_cast<uint8_t*>(stbi_load_16(absFilePath.c_str(), &width, &height, &channels, desiredChannelCount));
    } else if (channelSize == 4) {
        data = reinterpret_cast<uint8_t*>(stbi_loadf(absFilePath.c_str(), &width, &height, &channels, desiredChannelCount));
    } else {
        LOG_ERROR("Unable to load image \"%s\": Invalid image data format", absFilePath.c_str());
        return nullptr;
    }

    if (data == nullptr) {
        LOG_ERROR("Failed to load image \"%s\" - Reason: %s", absFilePath.c_str(), stbi_failure_reason());
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
        LOG_ERROR("Failed to load image \"%s\": Invalid pixel layout for %d channels", absFilePath.c_str(), channels);
        stbi_image_free(data);
        return nullptr;
    }

    ImagePixelFormat format =
            channelSize == 1 ? ImagePixelFormat::UInt8 :
            channelSize == 2 ? ImagePixelFormat::UInt16 :
            channelSize == 4 ? ImagePixelFormat::Float32 :
            ImagePixelFormat::Invalid;

    if (format == ImagePixelFormat::Invalid) {
        LOG_ERROR("Failed to load image \"%s\": Invalid pixel format for %d bytes per channel", absFilePath.c_str(), channelSize);
        stbi_image_free(data);
        return nullptr;
    }

    LOG_INFO("Finished loading image \"%s\" - Took %.2f msec", absFilePath.c_str(), Performance::milliseconds(t0));

    ImageData* image = new ImageData(data, width, height, layout, format, AllocationType_Stbi);

    s_imageCache.insert(std::make_pair(absFilePath, image));

    return image;
}

void ImageData::unload(const std::string& filePath) {
    LOG_INFO("Unloading image \"%s\"", filePath.c_str());
    auto it = s_imageCache.find(filePath);
    if (it != s_imageCache.end()) {
        delete it->second;
        s_imageCache.erase(it);
    }
}

void ImageData::clearCache() {
    s_imageCache.clear();
}

ImageData* ImageData::mutate(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat) {
    uint8_t* mutatedPixels;

    if (srcLayout == ImagePixelLayout::Invalid || dstLayout == ImagePixelLayout::Invalid) {
        LOG_ERROR("Unable to mutate image pixels: Invalid pixel layout");
        return nullptr;
    }

    if (srcFormat == ImagePixelFormat::Invalid || dstFormat == ImagePixelFormat::Invalid) {
        LOG_ERROR("Unable to mutate image pixels: Invalid pixel format");
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

        int swizzleTransform[4] = {0, 1, 2, 3};


        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                for (glm::length_t i = 0; i < srcChannels; ++i) {
                    switch (srcFormat) {
                        case ImagePixelFormat::UInt8:
                            channels[swizzleTransform[i]] = reinterpret_cast<uint8_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::SInt8:
                            channels[swizzleTransform[i]] = reinterpret_cast<int8_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::UInt16:
                            channels[swizzleTransform[i]] = reinterpret_cast<uint16_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::SInt16:
                            channels[swizzleTransform[i]] = reinterpret_cast<int16_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::UInt32:
                            channels[swizzleTransform[i]] = reinterpret_cast<uint32_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::SInt32:
                            channels[swizzleTransform[i]] = reinterpret_cast<int32_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::Float16:
                            channels[swizzleTransform[i]] = reinterpret_cast<uint16_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::Float32:
                            channels[swizzleTransform[i]] = reinterpret_cast<uint32_t*>(srcPixel)[i];
                            break;
                        case ImagePixelFormat::Invalid:
                            assert(false);
                    }
                }

                for (glm::length_t i = 0; i < dstChannels; ++i) {
                    switch (dstFormat) {
                        case ImagePixelFormat::UInt8:
                            reinterpret_cast<uint8_t*>(dstPixel)[i] = (uint8_t)channels[i];
                            break;
                        case ImagePixelFormat::SInt8:
                            reinterpret_cast<int8_t*>(dstPixel)[i] = (int8_t)channels[i];
                            break;
                        case ImagePixelFormat::UInt16:
                            reinterpret_cast<uint16_t*>(dstPixel)[i] = (uint16_t)channels[i];
                            break;
                        case ImagePixelFormat::SInt16:
                            reinterpret_cast<int16_t*>(dstPixel)[i] = (int16_t)channels[i];
                            break;
                        case ImagePixelFormat::UInt32:
                            reinterpret_cast<uint32_t*>(dstPixel)[i] = (uint32_t)channels[i];
                            break;
                        case ImagePixelFormat::SInt32:
                            reinterpret_cast<int32_t*>(dstPixel)[i] = (int32_t)channels[i];
                            break;
                        case ImagePixelFormat::Float16:
                            reinterpret_cast<uint16_t*>(dstPixel)[i] = (uint16_t)channels[i];
                            break;
                        case ImagePixelFormat::Float32:
                            reinterpret_cast<uint32_t*>(dstPixel)[i] = (uint32_t)channels[i];
                            break;
                        case ImagePixelFormat::Invalid:
                            assert(false);
                    }
                }

                srcPixel += srcStride;
                dstPixel += dstStride;
            }
        }
    }

    return new ImageData(mutatedPixels, width, height, dstLayout, dstFormat, AllocationType_Internal);
}

ImageData* ImageData::transform(const ImageData* imageData, const ImageTransform& transformation) {
    return transform(imageData->getData(), imageData->getWidth(), imageData->getHeight(), imageData->getPixelLayout(), imageData->getPixelFormat(), transformation);
}

ImageData* ImageData::transform(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format, const ImageTransform& transformation) {
    return transformation.apply(data, width, height, layout, format);
}

int64_t ImageData::getChannel(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex) {
    size_t channelOffset = ImageData::getChannelOffset(x, y, channelIndex, m_width, m_height, m_pixelLayout, m_pixelFormat);
    uint8_t* data = &m_data[channelOffset];

    switch (m_pixelFormat) {
        case ImagePixelFormat::UInt8:
            return (int64_t)(*(uint8_t*)(data));
        case ImagePixelFormat::SInt8:
            return (int64_t)(*(int8_t*)(data));
        case ImagePixelFormat::UInt16:
            return (int64_t)(*(uint16_t*)(data));
        case ImagePixelFormat::SInt16:
            return (int64_t)(*(int16_t*)(data));
        case ImagePixelFormat::UInt32:
            return (int64_t)(*(uint32_t*)(data));
        case ImagePixelFormat::SInt32:
            return (int64_t)(*(int32_t*)(data));
        case ImagePixelFormat::Float16:
            return (int64_t)(*(uint16_t*)(data)); // No c++ representation
        case ImagePixelFormat::Float32:
            return (int64_t)(*(float*)(data));
        default:
            assert(false);
            return 0;
    }
}

void ImageData::setChannel(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex, int64_t value) {
    size_t channelOffset = ImageData::getChannelOffset(x, y, channelIndex, m_width, m_height, m_pixelLayout, m_pixelFormat);
    uint8_t* data = &m_data[channelOffset];

    union {
        int64_t value;
        int8_t s8;
        uint8_t u8;
        int16_t s16;
        uint16_t u16;
        int32_t s32;
        uint32_t u32;
        uint16_t f16;
        float f32;
    } channel{};

    channel.value = value;

    switch (m_pixelFormat) {
        case ImagePixelFormat::UInt8:
            (*(uint8_t*)data) = channel.u8;
            break;
        case ImagePixelFormat::SInt8:
            (*(int8_t*)data) = channel.s8;
            break;
        case ImagePixelFormat::UInt16:
            (*(uint16_t*)data) = channel.u16;
            break;
        case ImagePixelFormat::SInt16:
            (*(int16_t*)data) = channel.s16;
            break;
        case ImagePixelFormat::UInt32:
            (*(uint32_t*)data) = channel.u32;
            break;
        case ImagePixelFormat::SInt32:
            (*(int32_t*)data) = channel.s32;
            break;
        case ImagePixelFormat::Float16:
            (*(uint16_t*)data) = channel.f16;
            break;
        case ImagePixelFormat::Float32:
            (*(float*)data) = channel.f32;
            break;
        default:
            assert(false);
    }
}

void ImageData::setPixel(ImageRegion::offset_type x, ImageRegion::offset_type y, int64_t r, int64_t g, int64_t b, int64_t a) {
    size_t numChannels = ImageData::getChannels(m_pixelLayout);
    vk::ComponentSwizzle swizzle[4];
    ImageData::getPixelSwizzle(m_pixelLayout, swizzle);

    int64_t ONE = 1;
    int64_t ZERO = 0;

    for (size_t i = 0; i < numChannels; ++i) {
        switch (swizzle[i]) {
            case vk::ComponentSwizzle::eR:
                setChannel(x, y, i, r);
                break;
            case vk::ComponentSwizzle::eG:
                setChannel(x, y, i, g);
                break;
            case vk::ComponentSwizzle::eB:
                setChannel(x, y, i, b);
                break;
            case vk::ComponentSwizzle::eA:
                setChannel(x, y, i, a);
                break;
//            case vk::ComponentSwizzle::eOne: setChannel(x, y, i, ONE); break;
//            case vk::ComponentSwizzle::eZero: setChannel(x, y, i, ZERO); break;
            default:
                break; // break from switch, not for loop
        }
    }
}

void ImageData::setPixelf(ImageRegion::offset_type x, ImageRegion::offset_type y, float r, float g, float b, float a) {
    assert(m_pixelFormat == ImagePixelFormat::Float32);

    size_t numChannels = ImageData::getChannels(m_pixelLayout);
    vk::ComponentSwizzle swizzle[4];
    ImageData::getPixelSwizzle(m_pixelLayout, swizzle);

    float fONE = 1.0F;
    float fZERO = 0.0F;
    int64_t ONE = *(int64_t*)&fONE;
    int64_t ZERO = *(int64_t*)&fZERO;

    for (size_t i = 0; i < numChannels; ++i) {
        switch (swizzle[i]) {
            case vk::ComponentSwizzle::eR:
                setChannel(x, y, i, *(int64_t*)&r);
                break;
            case vk::ComponentSwizzle::eG:
                setChannel(x, y, i, *(int64_t*)&g);
                break;
            case vk::ComponentSwizzle::eB:
                setChannel(x, y, i, *(int64_t*)&b);
                break;
            case vk::ComponentSwizzle::eA:
                setChannel(x, y, i, *(int64_t*)&a);
                break;
//            case vk::ComponentSwizzle::eOne: setChannel(x, y, i, ONE); break;
//            case vk::ComponentSwizzle::eZero: setChannel(x, y, i, ZERO); break;
            default:
                break; // break from switch, not for loop
        }
    }
}

uint8_t* ImageData::getData() const {
    return m_data;
}

ImageRegion::size_type ImageData::getWidth() const {
    return m_width;
}

ImageRegion::size_type ImageData::getHeight() const {
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
        case ImagePixelLayout::Invalid:
            break;
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
        case ImagePixelFormat::Invalid:
            break;
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
            swizzle[3] = vk::ComponentSwizzle::eA;
            return true;
        case ImagePixelLayout::ABGR:
            swizzle[0] = vk::ComponentSwizzle::eA;
            swizzle[1] = vk::ComponentSwizzle::eB;
            swizzle[2] = vk::ComponentSwizzle::eG;
            swizzle[3] = vk::ComponentSwizzle::eR;
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
        case vk::Format::eD16Unorm:
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
        case vk::Format::eD24UnormS8Uint: // Is this correct?
        case vk::Format::eD16UnormS8Uint:
            outLayout = ImagePixelLayout::R;
            outFormat = ImagePixelFormat::UInt32;
            return true;
        case vk::Format::eR32Sint:
            outLayout = ImagePixelLayout::R;
            outFormat = ImagePixelFormat::SInt32;
            return true;
        case vk::Format::eD32Sfloat:
//        case vk::Format::eD32SfloatS8Uint:
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

size_t ImageData::getChannelOffset(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat) {
    assert(x < width && y < height);
    size_t numChannels = ImageData::getChannels(pixelLayout);
    assert(channelIndex < numChannels);
    size_t channelSize = ImageData::getChannelSize(pixelFormat);
    size_t pixelSize = numChannels * channelSize;
    size_t pixelOffset = y * width + x;
    return pixelOffset * pixelSize + channelIndex * channelSize;
}


ImageData* ImageData::ImageTransform::apply(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format) const {
    // No-op implementation - The image just gets copied.

    size_t pixelStride = ImageData::getChannels(layout) * ImageData::getChannelSize(format);
    assert(pixelStride != 0);

    uint8_t* dstPixels = static_cast<uint8_t*>(malloc(height * width * pixelStride));
    memcpy(dstPixels, data, height * width * pixelStride);

    return new ImageData(dstPixels, width, height, layout, format, AllocationType_Internal);
}

bool ImageData::ImageTransform::isNoOp() const {
    return true;
}

void ImageData::ImageTransform::initComputeResources() {
    Engine::graphics()->commandPool()->allocateCommandBuffer("image_compute_buffer", {vk::CommandBufferLevel::ePrimary});
}

void ImageData::ImageTransform::destroyComputeResources() {
    for (const auto& it : s_transformComputePipelines)
        delete it.second;
}


ImageData::Flip::Flip(bool x, bool y):
        flip_x(x),
        flip_y(y) {
}

ImageData::Flip::Flip(const Flip& copy):
        flip_x(copy.flip_x),
        flip_y(copy.flip_y) {
}

ImageData* ImageData::Flip::apply(uint8_t* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format) const {
    // TODO: use compute shader for this

    if (isNoOp())
        return ImageTransform::apply(data, width, height, layout, format);

    if (layout == ImagePixelLayout::Invalid) {
        LOG_ERROR("Unable to transform image pixels: Invalid pixel layout");
        return nullptr;
    }

    if (format == ImagePixelFormat::Invalid) {
        LOG_ERROR("Unable to transform image pixels: Invalid pixel format");
        return nullptr;
    }

    size_t pixelStride = ImageData::getChannels(layout) * ImageData::getChannelSize(format);
    assert(pixelStride != 0);
    size_t rowStride = pixelStride * width;
    size_t numPixels = height * width;
    size_t numBytes = numPixels * pixelStride;
    uint8_t* srcPixels = data;
    uint8_t* dstPixels = static_cast<uint8_t*>(malloc(numBytes));

    uint8_t* src, * dst;

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

    return new ImageData(dstPixels, width, height, layout, format, AllocationType_Internal);
}

bool ImageData::Flip::isNoOp() const {
    return !flip_x && !flip_y;
}

ComputePipeline* ImageData::Flip::getComputePipeline() {
    constexpr const char* key = "TransformPipeline_ImageFlip";
    auto it = s_transformComputePipelines.find(key);
    if (it == s_transformComputePipelines.end()) {
        ComputePipelineConfiguration pipelineConfig{};
        pipelineConfig.device = Engine::graphics()->getDevice();
        pipelineConfig.computeShader = "shaders/util/imageTransform/compute_flip.glsl";
        ComputePipeline* pipeline = ComputePipeline::create(pipelineConfig, "ImageDataFlip-TransformComputePipelines");
        s_transformComputePipelines.insert(std::make_pair(key, pipeline));
        return pipeline;
    } else {
        return it->second;
    }
}


bool ImageUtil::isDepthAttachment(vk::Format format) {
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

bool ImageUtil::isStencilAttachment(vk::Format format) {
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

bool ImageUtil::isColourAttachment(vk::Format format) {
    return !isDepthAttachment(format) && !isStencilAttachment(format); // TODO: Is it correct to assume all format types that are not depth or stencil are colour?
}

vk::Format ImageUtil::selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (size_t i = 0; i < candidates.size(); ++i) {
        vk::Format format = candidates[i];
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if ((tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) ||
            (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)) {

            return format;
        }
    }

    return vk::Format::eUndefined;
}

bool ImageUtil::getImageFormatProperties(vk::Format format, vk::ImageType type, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageCreateFlags flags, vk::ImageFormatProperties* imageFormatProperties) {
    const vk::PhysicalDevice& physicalDevice = Engine::graphics()->getPhysicalDevice();

    vk::Result result;
    result = physicalDevice.getImageFormatProperties(format, type, tiling, usage, flags, imageFormatProperties);

    if (result != vk::Result::eSuccess) {
        if (result == vk::Result::eErrorFormatNotSupported) {
            LOG_ERROR("Image format %s is not supported for usage=%s, tiling=%s, flags=%s", vk::to_string(format).c_str(), vk::to_string(usage).c_str(), vk::to_string(tiling).c_str(), vk::to_string(flags).c_str());
        } else {
            LOG_ERROR("Failed to get image format properties (%s) for format=%s, usage=%s, tiling=%s, flags=%s", vk::to_string(result).c_str() , vk::to_string(format).c_str(), vk::to_string(usage).c_str(), vk::to_string(tiling).c_str(), vk::to_string(flags).c_str());
        }
        return false;
    }

    return true;
}


bool ImageUtil::validateImageCreateInfo(const vk::ImageCreateInfo& imageCreateInfo) {
    vk::ImageFormatProperties imageFormatProperties;
    if (!getImageFormatProperties(imageCreateInfo.format, imageCreateInfo.imageType, imageCreateInfo.tiling, imageCreateInfo.usage, imageCreateInfo.flags, &imageFormatProperties)) {
        LOG_ERROR("Image validation failed: Unable to get image format properties for the supplied image configuration");
        return false;
    }

    if (imageCreateInfo.extent.width > imageFormatProperties.maxExtent.width ||
        imageCreateInfo.extent.height > imageFormatProperties.maxExtent.height ||
        imageCreateInfo.extent.depth > imageFormatProperties.maxExtent.depth) {
        LOG_ERROR("Image validation failed: requested image extent [%d x %d x %d] is greater than the maximum supported extent [%d x %d x %d] for the supplied configuration",
               imageCreateInfo.extent.width, imageCreateInfo.extent.height, imageCreateInfo.extent.depth,
               imageFormatProperties.maxExtent.width, imageFormatProperties.maxExtent.height, imageFormatProperties.maxExtent.depth);
        return false;
    }

    if (imageCreateInfo.mipLevels > imageFormatProperties.maxMipLevels) {
        LOG_ERROR("Image validation failed: %d requested mip levels is greater than the maximum %d mip levels supported for the supplied configuration",
               imageCreateInfo.mipLevels, imageFormatProperties.maxMipLevels);
        return false;
    }

    if (imageCreateInfo.arrayLayers > imageFormatProperties.maxArrayLayers) {
        LOG_ERROR("Image validation failed: %d requested array layers is greater than the maximum %d array layers supported for the supplied configuration",
               imageCreateInfo.arrayLayers, imageFormatProperties.maxArrayLayers);
        return false;
    }

    // TODO: validate sampleCount
    // TODO: validate maxResourceSize

    return true;
}

bool ImageUtil::transitionLayout(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageSubresourceRange& subresourceRange, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    PROFILE_BEGIN_GPU_CMD("ImageUtil::transitionLayout", commandBuffer);
//    if (srcState == dstState)
//        return true;

    vk::ImageMemoryBarrier barrier;
    barrier.setImage(image);
    barrier.setOldLayout(srcState.layout);
    barrier.setNewLayout(dstState.layout);
    barrier.setSrcAccessMask(srcState.accessMask);
    barrier.setDstAccessMask(dstState.accessMask);
    barrier.setSrcQueueFamilyIndex(srcState.queueFamilyIndex);
    barrier.setDstQueueFamilyIndex(dstState.queueFamilyIndex);
    barrier.setSubresourceRange(subresourceRange);

    vk::PipelineStageFlags srcStageMask = srcState.pipelineStages;
    vk::PipelineStageFlags dstStageMask = dstState.pipelineStages;

    commandBuffer.pipelineBarrier(srcStageMask, dstStageMask, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    PROFILE_END_GPU_CMD("ImageUtil::transitionLayout", commandBuffer);
    return true;
}

bool ImageUtil::upload(const vk::Image& dstImage, void* data, uint32_t bytesPerPixel, vk::ImageAspectFlags aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {
    const vk::CommandBuffer& commandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::upload(commandBuffer, dstImage, data, bytesPerPixel, aspectMask, imageRegion, dstState, srcState);

    ImageUtil::endTransferCommands(**Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true);
    return success;
}

bool ImageUtil::upload(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, void* data, uint32_t bytesPerPixel, vk::ImageAspectFlags aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {

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

    if (imageRegion.width >= ImageRegion::WHOLE_SIZE
        || imageRegion.height >= ImageRegion::WHOLE_SIZE
        || imageRegion.depth >= ImageRegion::WHOLE_SIZE
        || imageRegion.layerCount >= ImageRegion::WHOLE_SIZE) {
        LOG_ERROR("Unable to upload Image: Image region is out of bounds");
        return false;
    }

    // TODO: we should limit the size of the i,age staging buffer, and upload the data in chunks if it does not all fit within the buffer.
    Buffer* srcBuffer = getImageStagingBuffer(imageRegion, bytesPerPixel);
    if (srcBuffer == nullptr) {
        LOG_ERROR("Unable to upload Image: Failed to get staging buffer");
        return false;
    }

    srcBuffer->upload(0, ImageUtil::getImageSizeBytes(imageRegion, bytesPerPixel), data);

    vk::BufferImageCopy imageCopy{};
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

    bool success = transferBuffer(commandBuffer, dstImage, srcBuffer->getBuffer(), imageCopy, dstState, srcState);

    return success;
}


bool ImageUtil::transferBuffer(const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {

    const vk::CommandBuffer& commandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::transferBuffer(commandBuffer, dstImage, srcBuffer, imageCopy, dstState, srcState);

    ImageUtil::endTransferCommands(**Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true);
    return success;
}

bool ImageUtil::transferBuffer(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {
    PROFILE_BEGIN_GPU_CMD("ImageUtil::transferBuffer", commandBuffer);

    vk::ImageSubresourceRange subresourceRange{};
    subresourceRange.setAspectMask(imageCopy.imageSubresource.aspectMask);
    subresourceRange.setBaseArrayLayer(imageCopy.imageSubresource.baseArrayLayer);
    subresourceRange.setLayerCount(imageCopy.imageSubresource.layerCount);
    subresourceRange.setBaseMipLevel(imageCopy.imageSubresource.mipLevel);
    subresourceRange.setLevelCount(1);

    ImageUtil::transitionLayout(commandBuffer, dstImage, subresourceRange, srcState, ImageTransition::TransferDst());
    commandBuffer.copyBufferToImage(srcBuffer, dstImage, vk::ImageLayout::eTransferDstOptimal, 1, &imageCopy);
    ImageUtil::transitionLayout(commandBuffer, dstImage, subresourceRange, ImageTransition::TransferDst(), dstState);

    PROFILE_END_GPU_CMD("ImageUtil::transferBuffer", commandBuffer);
    return true;
}

bool ImageUtil::generateMipmap(const vk::Image& image, vk::Format format, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t baseLayer, uint32_t layerCount, ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t mipLevels, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {

    const vk::CommandBuffer& commandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::generateMipmap(commandBuffer, image, format, filter, aspectMask, baseLayer, layerCount, width, height, depth, mipLevels, dstState, srcState);

    ImageUtil::endTransferCommands(**Engine::graphics()->getQueue(QUEUE_GRAPHICS_TRANSFER_MAIN), true);
    return success;
}

bool ImageUtil::generateMipmap(const vk::CommandBuffer& commandBuffer, const vk::Image& image, vk::Format format, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t baseLayer, uint32_t layerCount, ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t mipLevels, const ImageTransitionState& dstState, const ImageTransitionState& srcState) {
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::FormatFeatureFlags testFormatFeatureFlags{};
    if (filter == vk::Filter::eLinear) testFormatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
    if (filter == vk::Filter::eCubicEXT) testFormatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImageFilterCubicEXT;
    if (filter == vk::Filter::eCubicIMG) testFormatFeatureFlags |= vk::FormatFeatureFlagBits::eSampledImageFilterCubicIMG;

    if (testFormatFeatureFlags && !ImageUtil::checkAllImageFormatFeatures(format, tiling, testFormatFeatureFlags)) {
        LOG_ERROR("Unable to generate mipmap: Image format %s does not support filter type %s", vk::to_string(format).c_str(), vk::to_string(filter).c_str());
        return false;
    }

    int32_t levelWidth = glm::max((int32_t)width, 1);
    int32_t levelHeight = glm::max((int32_t)height, 1);
    int32_t levelDepth = glm::max((int32_t)depth, 1);
    uint32_t mipLevelCount = glm::clamp(mipLevels, 1u, ImageUtil::getMaxMipLevels(levelWidth, levelHeight, levelDepth));

    if (mipLevelCount == 1)
        return true;

    vk::ImageSubresourceRange subresourceRange{};
    subresourceRange.setAspectMask(aspectMask);
    subresourceRange.setBaseArrayLayer(baseLayer);
    subresourceRange.setLayerCount(layerCount);
    subresourceRange.setLevelCount(1);

    vk::ArrayWrapper1D<vk::Offset3D, 2> test;
    test[0] = vk::Offset3D(0, 0, 0);

    vk::ImageBlit blit{};
    blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
    blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
    blit.srcSubresource.aspectMask = aspectMask;
    blit.srcSubresource.baseArrayLayer = baseLayer;
    blit.srcSubresource.layerCount = layerCount;
    blit.dstSubresource.aspectMask = aspectMask;
    blit.dstSubresource.baseArrayLayer = baseLayer;
    blit.dstSubresource.layerCount = layerCount;

    PROFILE_BEGIN_GPU_CMD("ImageUtil::generateMipmap", commandBuffer);

    ImageTransitionState prevLevelState = srcState;

    for (uint32_t i = 1; i < mipLevelCount; ++i) {

        subresourceRange.setBaseMipLevel(i - 1);
        ImageUtil::transitionLayout(commandBuffer, image, subresourceRange, prevLevelState, ImageTransition::TransferSrc());

        subresourceRange.setBaseMipLevel(i);
        ImageUtil::transitionLayout(commandBuffer, image, subresourceRange, srcState, ImageTransition::TransferDst());
        prevLevelState = ImageTransition::TransferDst();

        blit.srcOffsets[1] = vk::Offset3D(levelWidth, levelHeight, levelDepth);
        blit.srcSubresource.mipLevel = i - 1;

        if (levelWidth > 1) levelWidth /= 2;
        if (levelHeight > 1) levelHeight /= 2;
        if (levelDepth > 1) levelDepth /= 2;

        blit.dstOffsets[1] = vk::Offset3D(levelWidth, levelHeight, levelDepth);
        blit.dstSubresource.mipLevel = i;

        commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, 1, &blit, filter);

        subresourceRange.setBaseMipLevel(i - 1);
        ImageUtil::transitionLayout(commandBuffer, image, subresourceRange, ImageTransition::TransferSrc(), dstState);

    }

    subresourceRange.setBaseMipLevel(mipLevelCount - 1);
    ImageUtil::transitionLayout(commandBuffer, image, subresourceRange, prevLevelState, dstState);

    PROFILE_END_GPU_CMD("ImageUtil::generateMipmap", commandBuffer);
    return true;
}

uint32_t ImageUtil::getMaxMipLevels(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth) {
    return static_cast<uint32_t>(glm::floor(glm::log2((float)glm::max(glm::max(width, height), depth)))) + 1;
}

bool ImageUtil::checkAllImageFormatFeatures(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags formatFeatureFlags) {
    const vk::PhysicalDevice& physicalDevice = Engine::graphics()->getPhysicalDevice();

    vk::FormatProperties formatProperties{};
    physicalDevice.getFormatProperties(format, &formatProperties);

    vk::FormatFeatureFlags testFlags =
            tiling == vk::ImageTiling::eOptimal ? formatProperties.optimalTilingFeatures :
            tiling == vk::ImageTiling::eLinear ? formatProperties.linearTilingFeatures :
            vk::FormatFeatureFlags(0);

    return (testFlags & formatFeatureFlags) == formatFeatureFlags; // All desired flag bits are set.
}

bool ImageUtil::checkAnyImageFormatFeatures(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags formatFeatureFlags) {
    const vk::PhysicalDevice& physicalDevice = Engine::graphics()->getPhysicalDevice();

    vk::FormatProperties formatProperties{};
    physicalDevice.getFormatProperties(format, &formatProperties);

    vk::FormatFeatureFlags testFlags =
            tiling == vk::ImageTiling::eOptimal ? formatProperties.optimalTilingFeatures :
            tiling == vk::ImageTiling::eLinear ? formatProperties.linearTilingFeatures :
            vk::FormatFeatureFlags(0);

    return !!(testFlags & formatFeatureFlags); // Any desired flag bits are set.
}

const vk::CommandBuffer& ImageUtil::beginTransferCommands() {
    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::getTransferCommandBuffer();

    vk::CommandBufferBeginInfo commandBeginInfo;
    commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    transferCommandBuffer.begin(commandBeginInfo);
    PROFILE_BEGIN_GPU_CMD("ImageUtil::beginTransferCommands", transferCommandBuffer);

    return transferCommandBuffer;
}

void ImageUtil::endTransferCommands(const vk::Queue& queue, bool waitComplete) {
    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::getTransferCommandBuffer();

    PROFILE_END_GPU_CMD("ImageUtil::beginTransferCommands", transferCommandBuffer);
    transferCommandBuffer.end();

    vk::SubmitInfo queueSumbitInfo;
    queueSumbitInfo.setCommandBufferCount(1);
    queueSumbitInfo.setPCommandBuffers(&transferCommandBuffer);
    const vk::Queue& graphicsTransferQueue = **Engine::graphics()->getQueue(QUEUE_GRAPHICS_TRANSFER_MAIN);
    vk::Result result = queue.submit(1, &queueSumbitInfo, nullptr);
    assert(result == vk::Result::eSuccess);
    if (waitComplete)
        queue.waitIdle();
}

const vk::CommandBuffer& ImageUtil::getTransferCommandBuffer() {
    if (!Engine::graphics()->commandPool()->hasCommandBuffer("image_transfer_buffer"))
        Engine::graphics()->commandPool()->allocateCommandBuffer("image_transfer_buffer", {vk::CommandBufferLevel::ePrimary});

    return **Engine::graphics()->commandPool()->getCommandBuffer("image_transfer_buffer");
}

const vk::CommandBuffer& ImageUtil::getComputeCommandBuffer() {
    if (!Engine::graphics()->commandPool()->hasCommandBuffer("image_compute_buffer"))
        Engine::graphics()->commandPool()->allocateCommandBuffer("image_compute_buffer", {vk::CommandBufferLevel::ePrimary});

    return **Engine::graphics()->commandPool()->getCommandBuffer("image_compute_buffer");
}

const vk::Queue& ImageUtil::getComputeQueue() {
    return **Engine::graphics()->getQueue(QUEUE_COMPUTE_MAIN);
}

vk::DeviceSize ImageUtil::getImageSizeBytes(const ImageRegion& imageRegion, uint32_t bytesPerPixel) {
    return ImageUtil::getImageSizeBytes(imageRegion.width, imageRegion.height, imageRegion.depth, bytesPerPixel);
}

vk::DeviceSize ImageUtil::getImageSizeBytes(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t bytesPerPixel) {
    return (vk::DeviceSize)width * (vk::DeviceSize)height * (vk::DeviceSize)depth * (vk::DeviceSize)bytesPerPixel;
}

Buffer* ImageUtil::getImageStagingBuffer(const ImageRegion& imageRegion, uint32_t bytesPerPixel) {
    return ImageUtil::getImageStagingBuffer(imageRegion.width, imageRegion.height, imageRegion.depth, bytesPerPixel);
}

void _ImageUtil_onCleanupGraphics(ShutdownGraphicsEvent* event) {
    LOG_INFO("Destroying image staging buffer");
    delete g_imageStagingBuffer;
}

Buffer* ImageUtil::getImageStagingBuffer(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t bytesPerPixel) {
    constexpr uint32_t maxImageDimension = 32768;
    if (width > maxImageDimension || height > maxImageDimension || depth > maxImageDimension || bytesPerPixel > 64) {
        LOG_ERROR("Unable to get image data staging buffer: Requested dimensions too large");
    }

    constexpr vk::DeviceSize maxSizeBytes = UINT32_MAX; // 4 GiB

    vk::DeviceSize requiredSize = ImageUtil::getImageSizeBytes(width, height, depth, bytesPerPixel);
    if (requiredSize > 1llu << 32) {
        char c1[6] = "", c2[6] = "";
        LOG_ERROR("Unable to get image data staging buffer: Requested dimensions exceed maximum allocatable size for staging buffer (Requested %.3f %s, maximum %.3f %s)",
               Util::getMemorySizeMagnitude(requiredSize, c1), c1, Util::getMemorySizeMagnitude(maxSizeBytes, c2), c2);
    }

    if (g_imageStagingBuffer == nullptr) {
        Engine::eventDispatcher()->connect(&_ImageUtil_onCleanupGraphics);
    }

    if (g_imageStagingBuffer == nullptr || g_imageStagingBuffer->getSize() < requiredSize) {
        delete g_imageStagingBuffer;

        char c1[6] = "";
        LOG_INFO("Resizing ImageData staging buffer to %.3f %s", Util::getMemorySizeMagnitude(requiredSize, c1), c1);

        BufferConfiguration bufferConfig{};
        bufferConfig.device = Engine::graphics()->getDevice();
        bufferConfig.size = requiredSize;
        bufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
        bufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        g_imageStagingBuffer = Buffer::create(bufferConfig, "ImageUtil-ImageStagingBuffer");
        if (g_imageStagingBuffer == nullptr) {
            LOG_ERROR("Failed to create image data staging buffer");
            return nullptr;
        }
    }

    return g_imageStagingBuffer;
}


ImageTransitionState::ImageTransitionState(vk::ImageLayout layout, vk::AccessFlagBits accessMask, vk::PipelineStageFlags pipelineStages, uint32_t queueFamilyIndex):
        layout(layout),
        accessMask(accessMask),
        pipelineStages(pipelineStages),
        queueFamilyIndex(queueFamilyIndex) {
}

bool ImageTransitionState::operator==(const ImageTransitionState& other) const {
    if (layout != other.layout) return false;
    if (pipelineStages != other.pipelineStages) return false;
    if (accessMask != other.accessMask) return false;
    if (queueFamilyIndex != other.queueFamilyIndex) return false;
    return true;
}


ImageTransition::FromAny::FromAny(vk::PipelineStageFlagBits shaderPipelineStages):
        ImageTransitionState(vk::ImageLayout::eUndefined, {}, shaderPipelineStages) {
}

ImageTransition::General::General(vk::PipelineStageFlagBits shaderPipelineStages):
        ImageTransitionState(vk::ImageLayout::eGeneral, {}, shaderPipelineStages) {
}

ImageTransition::TransferDst::TransferDst():
        ImageTransitionState(vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTransfer) {
}

ImageTransition::TransferSrc::TransferSrc():
        ImageTransitionState(vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferRead, vk::PipelineStageFlagBits::eTransfer) {
}

ImageTransition::ShaderAccess::ShaderAccess(vk::PipelineStageFlags shaderPipelineStages, bool read, bool write):
        ImageTransitionState(vk::ImageLayout::eShaderReadOnlyOptimal, {}, shaderPipelineStages) {
#if _DEBUG
    const vk::PipelineStageFlags validShaderStages =
            vk::PipelineStageFlagBits::eVertexShader |
            vk::PipelineStageFlagBits::eGeometryShader |
            vk::PipelineStageFlagBits::eTessellationControlShader |
            vk::PipelineStageFlagBits::eTessellationEvaluationShader |
            vk::PipelineStageFlagBits::eFragmentShader |
            vk::PipelineStageFlagBits::eComputeShader |
            vk::PipelineStageFlagBits::eRayTracingShaderNV |
            vk::PipelineStageFlagBits::eTaskShaderNV |
            vk::PipelineStageFlagBits::eMeshShaderNV;
    if ((shaderPipelineStages & (~validShaderStages))) {
        LOG_FATAL("Provided pipeline stages for ShaderAccess image transition must only contain shader stages");
        assert(false);
    }
    if (!read && !write) {
        LOG_FATAL("Provided access flags for ShaderAccess image transition must be readable, writable or both");
        assert(false);
    }
#endif
    layout = write ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal;
    if (read) accessMask |= vk::AccessFlagBits::eShaderRead;
    if (write) accessMask |= vk::AccessFlagBits::eShaderWrite;
}

ImageTransition::ShaderReadOnly::ShaderReadOnly(vk::PipelineStageFlags shaderPipelineStages):
        ShaderAccess(shaderPipelineStages, true, false) {
}

ImageTransition::ShaderWriteOnly::ShaderWriteOnly(vk::PipelineStageFlags shaderPipelineStages):
        ShaderAccess(shaderPipelineStages, false, true) {
}

ImageTransition::ShaderReadWrite::ShaderReadWrite(vk::PipelineStageFlags shaderPipelineStages):
        ShaderAccess(shaderPipelineStages, true, true) {
}

ImageTransition::PresentSrc::PresentSrc():
        ImageTransitionState(vk::ImageLayout::ePresentSrcKHR, {}, vk::PipelineStageFlagBits::eBottomOfPipe) {
}
