#include "core/graphics/Image2D.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Engine.h"
#include "core/util/Logger.h"


void Image2DConfiguration::setSize(uint32_t p_width, uint32_t p_height) {
    width = p_width;
    height = p_height;
}

void Image2DConfiguration::setSize(const glm::uvec2& size) {
    setSize(size.x, size.y);
}

void Image2DConfiguration::setSize(const vk::Extent2D& size) {
    setSize(size.width, size.height);
}

void Image2DConfiguration::setSource(const ImageData* p_imageData) {
    imageData = p_imageData;
    filePath = "";
}

void Image2DConfiguration::setSource(const std::string& p_filePath) {
    imageData = nullptr;
    filePath = p_filePath;
}

Image2D::Image2D(const WeakResource<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, uint32_t width, uint32_t height, uint32_t mipLevelCount, vk::Format format, const std::string& name):
        GraphicsResource(ResourceType_Image2D, device, name),
        m_image(image),
        m_memory(memory),
        m_size(width, height),
        m_mipLevelCount(mipLevelCount),
        m_format(format) {
}

Image2D::~Image2D() {
    (**m_device).destroyImage(m_image);
    vfree(m_memory);
}

Image2D* Image2D::create(const Image2DConfiguration& image2DConfiguration, const std::string& name) {
    const vk::Device& device = **image2DConfiguration.device.lock(name);

    vk::Result result;

    uint32_t width = 0;
    uint32_t height = 0;

    const ImageData* imageData = image2DConfiguration.imageData;
    if (imageData == nullptr) {
        if (!image2DConfiguration.filePath.empty()) {
            // This image data will stay loaded
            ImagePixelLayout pixelLayout;
            ImagePixelFormat pixelFormat;
            if (!ImageData::getPixelLayoutAndFormat(image2DConfiguration.format, pixelLayout, pixelFormat)) {
                LOG_ERROR("Unable to create Image2D \"%s\": supplied image format %s has no corresponding loadable pixel format or layout", name.c_str(), vk::to_string(image2DConfiguration.format).c_str());
                return nullptr;
            }
            imageData = ImageData::load(image2DConfiguration.filePath, pixelLayout, pixelFormat);
            if (imageData == nullptr) {
                return nullptr;
            }
        }
    }

    if (imageData != nullptr) {
        width = imageData->getWidth();
        height = imageData->getHeight();
    } else {
        width = image2DConfiguration.width;
        height = image2DConfiguration.height;
    }

    vk::ImageUsageFlags usage = image2DConfiguration.usage;
    if (imageData != nullptr) {
        usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    bool generateMipmap = image2DConfiguration.generateMipmap && image2DConfiguration.mipLevels > 1;

    if (generateMipmap) {
        usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
    }

    uint32_t maxMipLevels = ImageUtil::getMaxMipLevels(width, height, 1);
    uint32_t mipLevels = glm::clamp(image2DConfiguration.mipLevels, 1u, maxMipLevels);

    vk::ImageCreateFlags imageCreateFlags = {};
    if (image2DConfiguration.mutableFormat)
        imageCreateFlags |= vk::ImageCreateFlagBits::eMutableFormat;

    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.setFlags(imageCreateFlags);
    imageCreateInfo.setImageType(vk::ImageType::e2D);
    imageCreateInfo.setFormat(image2DConfiguration.format);
    imageCreateInfo.extent.setWidth(width);
    imageCreateInfo.extent.setHeight(height);
    imageCreateInfo.extent.setDepth(1);
    imageCreateInfo.setMipLevels(mipLevels);
    imageCreateInfo.setArrayLayers(1);
    imageCreateInfo.setSamples(image2DConfiguration.sampleCount);
    imageCreateInfo.setTiling(image2DConfiguration.enabledTexelAccess ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal);
    imageCreateInfo.setUsage(usage);
    imageCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    imageCreateInfo.setInitialLayout(image2DConfiguration.preInitialized ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined);

    if (!ImageUtil::validateImageCreateInfo(imageCreateInfo)) {
        LOG_ERROR("Failed to create Image2D \"%s\": Image validation failed", name.c_str());
        return nullptr;
    }

    vk::Image image = nullptr;
    result = device.createImage(&imageCreateInfo, nullptr, &image);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create Image2D \"%s\": %s", name.c_str(), vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkImage)image, vk::ObjectType::eImage, name);

    const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, image2DConfiguration.memoryProperties, name);
    if (memory == nullptr) {
        device.destroyImage(image);
        LOG_ERROR("Failed to create Image2D \"%s\": Memory allocation failed", name.c_str());
        return nullptr;
    }

    memory->bindImage(image);

    Image2D* returnImage = new Image2D(image2DConfiguration.device, image, memory, width, height, mipLevels, imageCreateInfo.format, name);

    if (imageData != nullptr) {
        ImageRegion uploadRegion;
        uploadRegion.width = imageData->getWidth();
        uploadRegion.height = imageData->getHeight();
        ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);

        LOG_INFO("Uploading data for Image2D \"%s\": Size [%d x %d]", name.c_str(), (int32_t)uploadRegion.width, (int32_t)uploadRegion.height);

        if (!returnImage->upload(imageData->getData(), imageData->getPixelLayout(), imageData->getPixelFormat(), vk::ImageAspectFlagBits::eColor, uploadRegion, ImageTransition::FromAny(), dstState, 0)) {
            LOG_ERROR("Failed to create Image2D \"%s\": Failed to upload buffer data", name.c_str());
            delete returnImage;
            return nullptr;
        }

        if (generateMipmap) {
            returnImage->generateMipmap(vk::Filter::eLinear, vk::ImageAspectFlagBits::eColor, image2DConfiguration.mipLevels, ImageTransition::FromAny(), dstState);
        }
    } else if (generateMipmap) {
        LOG_WARN("GenerateMipmap requested for Image2D \"%s\", but no source data was uploaded to generate from", name.c_str());
    }

    return returnImage;
}

bool Image2D::upload(Image2D* dstImage, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& srcState, const ImageTransitionState& dstState, int a) {

    assert(dstImage != nullptr);
    assert(data != nullptr);

    if (pixelLayout == ImagePixelLayout::Invalid || pixelLayout >= ImagePixelLayout::Count) {
        LOG_ERROR("Unable to upload data for Image2D \"%s\": Invalid image pixel layout", dstImage->getName().c_str());
        return false;
    }

    if (pixelFormat == ImagePixelFormat::Invalid || pixelFormat >= ImagePixelFormat::Count) {
        LOG_ERROR("Unable to upload data for Image2D \"%s\": Invalid image pixel format", dstImage->getName().c_str());
        return false;
    }

    ImagePixelLayout dstPixelLayout;
    ImagePixelFormat dstPixelFormat;
    if (!ImageData::getPixelLayoutAndFormat(dstImage->getFormat(), dstPixelLayout, dstPixelFormat)) {
        LOG_ERROR("Unable to upload data for Image2D \"%s\": There is no corresponding pixel layout or format for the destination images format %s", dstImage->getName().c_str(), vk::to_string(dstImage->getFormat()).c_str());
        return false;
    }

    if (!validateImageRegion(dstImage, imageRegion)) {
        return false;
    }

    void* uploadData = data;
    ImageData* tempImageData = nullptr;

    if (dstPixelFormat != pixelFormat || dstPixelLayout != pixelLayout) {
        tempImageData = ImageData::mutate(static_cast<uint8_t*>(data), imageRegion.width, imageRegion.height, pixelLayout, pixelFormat, dstPixelLayout, dstPixelFormat);
        uploadData = tempImageData->getData();
    }

    uint32_t bytesPerPixel = ImageData::getChannelSize(dstPixelFormat) * ImageData::getChannels(dstPixelLayout);

    if (bytesPerPixel == 0) {
        LOG_ERROR("Unable to upload data for Image2D \"%s\": Invalid image pixel format", dstImage->getName().c_str());
        delete tempImageData;
        return false;
    }

    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::upload(transferCommandBuffer, dstImage->getImage(), uploadData, bytesPerPixel, aspectMask, imageRegion, srcState, dstState);

    if (dstImage->getMipLevelCount() > 1) {
        // Transition all other mip levels to dstState, since transferBuffer only transitions imageCopy.imageSubresource.mipLevel
        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange.setAspectMask(aspectMask);
        subresourceRange.setBaseArrayLayer(imageRegion.baseLayer);
        subresourceRange.setLayerCount(imageRegion.layerCount);
        subresourceRange.setBaseMipLevel(0);
        subresourceRange.setLevelCount(dstImage->getMipLevelCount());
        ImageUtil::transitionLayout(transferCommandBuffer, dstImage->getImage(), subresourceRange, ImageTransition::FromAny(), dstState);
    }

    ImageUtil::endTransferCommands(transferCommandBuffer, **Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true, nullptr);

    delete tempImageData;
    return success;
}

bool Image2D::upload(void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& srcState, const ImageTransitionState& dstState, int a) {
    return Image2D::upload(this, data, pixelLayout, pixelFormat, aspectMask, imageRegion, srcState, dstState, a);
}

bool Image2D::readPixels(Image2D* srcImage, void* dstPixels, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    assert(srcImage != nullptr);
    assert(dstPixels != nullptr);

    if (pixelLayout == ImagePixelLayout::Invalid || pixelLayout >= ImagePixelLayout::Count) {
        LOG_ERROR("Unable to read pixel data for Image2D \"%s\": Invalid image pixel layout", srcImage->getName().c_str());
        return false;
    }

    if (pixelFormat == ImagePixelFormat::Invalid || pixelFormat >= ImagePixelFormat::Count) {
        LOG_ERROR("Unable to read pixel data for Image2D \"%s\": Invalid image pixel format", srcImage->getName().c_str());
        return false;
    }

    ImagePixelLayout srcPixelLayout;
    ImagePixelFormat srcPixelFormat;
    if (!ImageData::getPixelLayoutAndFormat(srcImage->getFormat(), srcPixelLayout, srcPixelFormat)) {
        LOG_ERROR("Unable to read pixel data for Image2D \"%s\": There is no corresponding pixel layout or format for the destination images format %s", srcImage->getName().c_str(), vk::to_string(srcImage->getFormat()).c_str());
        return false;
    }

    if (!validateImageRegion(srcImage, imageRegion)) {
        return false;
    }

    uint32_t bytesPerPixel = ImageData::getChannelSize(srcPixelFormat) * ImageData::getChannels(srcPixelLayout);

    if (bytesPerPixel == 0) {
        LOG_ERROR("Unable to read pixel data for Image2D \"%s\": Invalid image pixel format", srcImage->getName().c_str());
        return false;
    }

    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::beginTransferCommands();

    vk::BufferImageCopy imageCopy{};
    imageCopy.setBufferOffset(0);
    imageCopy.setBufferRowLength(0);
    imageCopy.setBufferImageHeight(0);
    imageCopy.imageSubresource.setAspectMask(aspectMask);
    imageCopy.imageSubresource.setMipLevel(imageRegion.baseMipLevel);
    imageCopy.imageSubresource.setBaseArrayLayer(imageRegion.baseLayer);
    imageCopy.imageSubresource.setLayerCount(imageRegion.layerCount);
    imageCopy.imageOffset.setX((int32_t)imageRegion.x);
    imageCopy.imageOffset.setY((int32_t)imageRegion.y);
    imageCopy.imageOffset.setZ((int32_t)imageRegion.z);
    imageCopy.imageExtent.setWidth(imageRegion.width);
    imageCopy.imageExtent.setHeight(imageRegion.height);
    imageCopy.imageExtent.setDepth(imageRegion.depth);

    Buffer* buffer = ImageUtil::getImageStagingBuffer(imageRegion, bytesPerPixel);

    bool success = ImageUtil::transferImageToBuffer(transferCommandBuffer, buffer->getBuffer(), srcImage->getImage(), imageCopy, srcState, dstState, 0);

    ImageUtil::endTransferCommands(transferCommandBuffer, **Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true, nullptr);

    if (srcPixelFormat != pixelFormat || srcPixelLayout != pixelLayout) {
        ImageData* tempImageData = ImageData::mutate(static_cast<uint8_t*>(buffer->map()), imageRegion.width, imageRegion.height, srcPixelLayout, srcPixelFormat, pixelLayout, pixelFormat);
        memcpy(dstPixels, tempImageData->getData(), ImageUtil::getImageSizeBytes(imageRegion, bytesPerPixel));
        delete tempImageData;

    } else {
        memcpy(dstPixels, buffer->map(), ImageUtil::getImageSizeBytes(imageRegion, bytesPerPixel));
    }

    return success;
}

bool Image2D::readPixels(void* dstPixels, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    return Image2D::readPixels(this, dstPixels, pixelLayout, pixelFormat, aspectMask, imageRegion, srcState, dstState);
}

bool Image2D::generateMipmap(Image2D* image, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    const vk::CommandBuffer& commandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::generateMipmap(commandBuffer, image->getImage(), image->getFormat(), filter, aspectMask, 0, 1, image->getWidth(), image->getHeight(), 1, mipLevels, srcState, dstState, 0);

    ImageUtil::endTransferCommands(commandBuffer, **Engine::graphics()->getQueue(QUEUE_GRAPHICS_TRANSFER_MAIN), true, nullptr);
    return success;
}

bool Image2D::generateMipmap(vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& srcState, const ImageTransitionState& dstState) {
    return Image2D::generateMipmap(this, filter, aspectMask, mipLevels, srcState, dstState);
}

const vk::Image& Image2D::getImage() const {
    return m_image;
}

uint32_t Image2D::getWidth() const {
    return m_size.y;
}

uint32_t Image2D::getHeight() const {
    return m_size.x;
}

glm::uvec2 Image2D::getSize() const {
    return m_size;
}

uint32_t Image2D::getMipLevelCount() const {
    return m_mipLevelCount;
}

vk::Format Image2D::getFormat() const {
    return m_format;
}

bool Image2D::validateImageRegion(const Image2D* image, ImageRegion& imageRegion) {
    if (image == nullptr) {
        return false;
    }

    if ((ImageRegion::size_type)imageRegion.x >= image->getWidth() || (ImageRegion::size_type)imageRegion.y >= image->getHeight()) {
        LOG_ERROR("Image region out of range for Image2D \"%s\"", image->getName().c_str());
        return false;
    }

    if (imageRegion.width == ImageRegion::WHOLE_SIZE) imageRegion.width = image->getWidth() - imageRegion.x;
    if (imageRegion.height == ImageRegion::WHOLE_SIZE) imageRegion.height = image->getHeight() - imageRegion.y;
    if (imageRegion.mipLevelCount == ImageRegion::WHOLE_SIZE) imageRegion.mipLevelCount = 1;
    imageRegion.z = 0;
    imageRegion.depth = 1;
    imageRegion.baseLayer = 0;
    imageRegion.layerCount = 1;

    if (imageRegion.x + imageRegion.width > image->getWidth() || imageRegion.y + imageRegion.height > image->getHeight()) {
        LOG_ERROR("Image region out of range for Image2D \"%s\"", image->getName().c_str());
        return false;
    }

    return true;
}



