#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Application.h"




void Image2DConfiguration::setSize(const uint32_t& width, const uint32_t& height) {
    this->width = width;
    this->height = height;
}

void Image2DConfiguration::setSize(const glm::uvec2& size) {
    setSize(size.x, size.y);
}

void Image2DConfiguration::setSize(const vk::Extent2D& size) {
    setSize(size.width, size.height);
}

void Image2DConfiguration::setSource(const ImageData* imageData) {
    this->imageData = imageData;
    this->filePath = "";
}

void Image2DConfiguration::setSource(const std::string& filePath) {
    this->imageData = nullptr;
    this->filePath = filePath;
}

Image2D::Image2D(const std::weak_ptr<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& width, const uint32_t& height, const vk::Format& format):
        m_device(device),
        m_image(image),
        m_memory(memory),
        m_width(width),
        m_height(height),
        m_format(format),
        m_ResourceId(GraphicsManager::nextResourceId()) {
    //printf("Create image\n");
}

Image2D::~Image2D() {
    //printf("Destroying image\n");
    (**m_device).destroyImage(m_image);
    vfree(m_memory);
}

Image2D* Image2D::create(const Image2DConfiguration& image2DConfiguration) {
    const vk::Device& device = **image2DConfiguration.device.lock();

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

    if (!ImageUtil::validateImageCreateInfo(imageCreateInfo)) {
        return nullptr;
    }

    vk::Image image = VK_NULL_HANDLE;
    result = device.createImage(&imageCreateInfo, nullptr, &image);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create image: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, image2DConfiguration.memoryProperties);
    if (memory == nullptr) {
        device.destroyImage(image);
        printf("Image memory allocation failed\n");
        return nullptr;
    }

    memory->bindImage(image);

    Image2D* returnImage = new Image2D(image2DConfiguration.device, image, memory, width, height, imageCreateInfo.format);

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

    bool success = ImageUtil::upload(dstImage->getImage(), uploadData, bytesPerPixel, aspectMask, imageRegion, dstState);
    delete tempImageData;
    return success;
}

bool Image2D::upload(void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
    return Image2D::upload(this, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

std::shared_ptr<vkr::Device> Image2D::getDevice() const {
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

    if (imageRegion.x >= image->getWidth() || imageRegion.y >= image->getHeight()) {
        printf("Image region out of range\n");
        return false;
    }

    if (imageRegion.width == VK_WHOLE_SIZE) imageRegion.width = image->getWidth() - imageRegion.x;
    if (imageRegion.height == VK_WHOLE_SIZE) imageRegion.height = image->getHeight() - imageRegion.y;
    if (imageRegion.mipLevelCount == VK_WHOLE_SIZE) imageRegion.mipLevelCount = 1;
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



void ImageView2DConfiguration::setImage(const vk::Image& image) {
    assert(image);
    this->image = image;
}

void ImageView2DConfiguration::setImage(const Image2D* image) {
    assert(image != nullptr);
    setImage(image->getImage());
}

ImageView2D::ImageView2D(std::weak_ptr<vkr::Device> device, vk::ImageView imageView):
        m_device(device),
        m_imageView(imageView),
        m_resourceId(GraphicsManager::nextResourceId()) {
    //printf("Create ImageView\n");
}

ImageView2D::~ImageView2D() {
    //printf("Destroy ImageView\n");
    (**m_device).destroyImageView(m_imageView);
}

ImageView2D* ImageView2D::create(const ImageView2DConfiguration& imageView2DConfiguration) {
    const vk::Device& device = **imageView2DConfiguration.device.lock();

    if (!imageView2DConfiguration.image) {
        printf("Unable to create ImageView2D: Image is NULL\n");
        return nullptr;
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
    result = device.createImageView(&imageViewCreateInfo, nullptr, &imageView);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create ImageView2D: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    return new ImageView2D(imageView2DConfiguration.device, imageView);
}

std::shared_ptr<vkr::Device> ImageView2D::getDevice() const {
    return m_device;
}

const vk::ImageView& ImageView2D::getImageView() const {
    return m_imageView;
}

const GraphicsResource& ImageView2D::getResourceId() const {
    return m_resourceId;
}

