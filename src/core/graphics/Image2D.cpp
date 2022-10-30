#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Engine.h"




void Image2DConfiguration::setSize(const uint32_t& p_width, const uint32_t& p_height) {
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

Image2D::Image2D(const WeakResource<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& width, const uint32_t& height, const uint32_t& mipLevelCount, const vk::Format& format, const std::string& name):
        m_device(device, name),
        m_image(image),
        m_memory(memory),
        m_width(width),
        m_height(height),
        m_mipLevelCount(mipLevelCount),
        m_format(format),
        m_ResourceId(GraphicsManager::nextResourceId()) {
    //printf("Create image\n");
}

Image2D::~Image2D() {
    //printf("Destroying image\n");
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
                printf("Unable to create image: supplied image format %s has no corresponding loadable pixel format or layout\n", vk::to_string(image2DConfiguration.format).c_str());
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
        return nullptr;
    }

    vk::Image image = VK_NULL_HANDLE;
    result = device.createImage(&imageCreateInfo, nullptr, &image);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create image: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkImage)image, vk::ObjectType::eImage, name);

    const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, image2DConfiguration.memoryProperties);
    if (memory == nullptr) {
        device.destroyImage(image);
        printf("Image memory allocation failed\n");
        return nullptr;
    }

    memory->bindImage(image);

    Image2D* returnImage = new Image2D(image2DConfiguration.device, image, memory, width, height, mipLevels, imageCreateInfo.format, name);

    if (imageData != nullptr) {
        ImageRegion uploadRegion;
        uploadRegion.width = imageData->getWidth();
        uploadRegion.height = imageData->getHeight();
        ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);

        if (!returnImage->upload(imageData->getData(), imageData->getPixelLayout(), imageData->getPixelFormat(), vk::ImageAspectFlagBits::eColor, uploadRegion, dstState)) {
            printf("Failed to upload buffer data\n");
            delete returnImage;
            return nullptr;
        }

        if (generateMipmap) {
            returnImage->generateMipmap(vk::Filter::eLinear, vk::ImageAspectFlagBits::eColor, image2DConfiguration.mipLevels, dstState);
        }
    }

    return returnImage;
}

bool Image2D::upload(Image2D* dstImage, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {

    assert(dstImage != nullptr);
    assert(data != nullptr);

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
    ImageData* tempImageData = nullptr;

    if (dstPixelFormat != pixelFormat || dstPixelLayout != pixelLayout) {
        tempImageData = ImageData::mutate(static_cast<uint8_t*>(data), imageRegion.width, imageRegion.height, pixelLayout, pixelFormat, dstPixelLayout, dstPixelFormat);
        uploadData = tempImageData->getData();
    }

    uint32_t bytesPerPixel = ImageData::getChannelSize(dstPixelFormat) * ImageData::getChannels(dstPixelLayout);

    if (bytesPerPixel == 0) {
        printf("Unable to upload image data: Invalid image pixel format\n");
        delete tempImageData;
        return false;
    }

    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::beginTransferCommands();

    bool success = ImageUtil::upload(transferCommandBuffer, dstImage->getImage(), uploadData, bytesPerPixel, aspectMask, imageRegion, dstState);

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

    ImageUtil::endTransferCommands(**Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true);

    delete tempImageData;
    return success;
}

bool Image2D::upload(void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
    return Image2D::upload(this, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

bool Image2D::generateMipmap(Image2D* image, const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState) {
    return ImageUtil::generateMipmap(image->getImage(), image->getFormat(), filter, aspectMask, 0, 1, image->getWidth(), image->getHeight(), 1, mipLevels, dstState);
}

bool Image2D::generateMipmap(const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState) {
    return Image2D::generateMipmap(this, filter, aspectMask, mipLevels, dstState);
}

const SharedResource<vkr::Device>& Image2D::getDevice() const {
    return m_device;
}

const vk::Image& Image2D::getImage() const {
    return m_image;
}

const uint32_t& Image2D::getWidth() const {
    return m_width;
}

const uint32_t& Image2D::getHeight() const {
    return m_height;
}

glm::uvec2 Image2D::getResolution() const {
    return glm::uvec2(m_width, m_height);
}

const uint32_t Image2D::getMipLevelCount() const {
    return m_mipLevelCount;
}

const vk::Format& Image2D::getFormat() const {
    return m_format;
}

const GraphicsResource& Image2D::getResourceId() const {
    return m_ResourceId;
}

bool Image2D::validateImageRegion(const Image2D* image, ImageRegion& imageRegion) {
    if (image == nullptr) {
        return false;
    }

    if ((ImageRegion::size_type)imageRegion.x >= image->getWidth() || (ImageRegion::size_type)imageRegion.y >= image->getHeight()) {
        printf("Image region out of range\n");
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
        printf("Image region out of range\n");
        return false;
    }

    return true;
}



