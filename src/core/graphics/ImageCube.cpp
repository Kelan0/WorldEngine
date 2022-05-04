
#include "core/graphics/ImageCube.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Application.h"



void ImageCubeConfiguration::setSource(const std::array<ImageData*, 6>& faceImageData, const std::array<ImageData::ImageTransform*, 6>& faceImageTransforms) {
    for (size_t i = 0; i < faceImageData.size(); ++i)
        setSource((ImageCubeFace)i, faceImageData[i], faceImageTransforms[i]);
}

void ImageCubeConfiguration::setSource(const ImageCubeFace& face, ImageData* faceImageData, ImageData::ImageTransform* faceImageTransform) {
    this->faceImageData[face] = faceImageData;
    this->faceImageTransforms[face] = faceImageTransform;
    this->faceFilePath[face] = "";
    this->equirectangularFilePath = "";
}

void ImageCubeConfiguration::setSource(const std::array<std::string, 6>& faceFilePaths, const std::array<ImageData::ImageTransform*, 6>& faceImageTransforms) {
    for (size_t i = 0; i < faceFilePaths.size(); ++i)
        setSource((ImageCubeFace)i, faceFilePaths[i], faceImageTransforms[i]);
}

void ImageCubeConfiguration::setSource(const ImageCubeFace& face, const std::string& faceFilePath, ImageData::ImageTransform* faceImageTransform) {
    this->faceFilePath[face] = faceFilePath;
    this->faceImageTransforms[face] = faceImageTransform;
    this->faceImageData[face] = nullptr;
    this->equirectangularFilePath = "";
}

void ImageCubeConfiguration::setSource(const std::string& equirectangularFilePath) {
    for (size_t i = 0; i < 6; ++i) {
        this->faceImageData[i] = nullptr;
        this->faceFilePath[i] = "";
    }
    this->equirectangularFilePath = equirectangularFilePath;
}



ImageCube::ImageCube(const std::weak_ptr<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& size, const vk::Format& format):
        m_device(device),
        m_image(image),
        m_memory(memory),
        m_size(size),
        m_format(format),
        m_ResourceId(GraphicsManager::nextResourceId()) {

}

ImageCube::~ImageCube() {
    (**m_device).destroyImage(m_image);
    vfree(m_memory);
}

ImageCube* ImageCube::create(const ImageCubeConfiguration& imageCubeConfiguration) {
    const vk::Device& device = **imageCubeConfiguration.device.lock();

    vk::Result result;

    std::vector<ImageData*> allocatedImageData;
    std::array<ImageData*, 6> cubeFacesImageData = imageCubeConfiguration.faceImageData; // Copy
    std::array<bool, 6> loadedFaces = loadCubeFacesImageData(imageCubeConfiguration.faceFilePath, imageCubeConfiguration.format, cubeFacesImageData, allocatedImageData);

    bool suppliedFaceData = loadedFaces[0];

    for (size_t i = 0; i < 6; ++i) {
        if (suppliedFaceData && !loadedFaces[i]) {
            printf("Unable to create CubeImage: Failed to load all faces from supplied file paths\n");
            for (const auto& imageData : allocatedImageData) delete imageData;
            return nullptr;
        }
    }

    uint32_t size;

    if (suppliedFaceData) {
        size = cubeFacesImageData[0]->getWidth();

        for (size_t i = 0; i < 6; ++i) {
            if (cubeFacesImageData[i]->getWidth() != cubeFacesImageData[i]->getHeight()) {
                printf("Unable to load CubeImage: Supplied image data is now square. Width must equal height for all faces\n");
                for (const auto& imageData : allocatedImageData) delete imageData;
                return nullptr;
            }

            if (cubeFacesImageData[i]->getWidth() != size) {
                printf("Unable to load CubeImage: Supplied image data has mismatched sizes. All faces must have the same size\n");
                for (const auto& imageData : allocatedImageData) delete imageData;
                return nullptr;
            }
        }

    } else {
        size = imageCubeConfiguration.size;
    }

    for (size_t i = 0; i < 6; ++i) {
        if (imageCubeConfiguration.faceImageTransforms[i] != nullptr && !imageCubeConfiguration.faceImageTransforms[i]->isNoOp()) {
            ImageData* transformedImageData = ImageData::transform(cubeFacesImageData[i], *imageCubeConfiguration.faceImageTransforms[i]);
            delete cubeFacesImageData[i];
            cubeFacesImageData[i] = transformedImageData;
        }
    }

    vk::ImageUsageFlags usage = imageCubeConfiguration.usage;
    if (suppliedFaceData) {
        usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
    imageCreateInfo.setImageType(vk::ImageType::e2D);
    imageCreateInfo.setFormat(imageCubeConfiguration.format);
    imageCreateInfo.extent.setWidth(size);
    imageCreateInfo.extent.setHeight(size);
    imageCreateInfo.extent.setDepth(1);
    imageCreateInfo.setMipLevels(imageCubeConfiguration.mipLevels);
    imageCreateInfo.setArrayLayers(6);
    imageCreateInfo.setSamples(imageCubeConfiguration.sampleCount);
    imageCreateInfo.setTiling(imageCubeConfiguration.enabledTexelAccess ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal);
    imageCreateInfo.setUsage(usage);
    imageCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    imageCreateInfo.setInitialLayout(imageCubeConfiguration.preInitialized ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eUndefined);

    if (!ImageUtil::validateImageCreateInfo(imageCreateInfo)) {
        for (const auto& imageData : allocatedImageData) delete imageData;
        return nullptr;
    }

    vk::Image image = VK_NULL_HANDLE;
    result = device.createImage(&imageCreateInfo, nullptr, &image);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create image: %s\n", vk::to_string(result).c_str());
        for (const auto& imageData : allocatedImageData) delete imageData;
        return nullptr;
    }

    const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, imageCubeConfiguration.memoryProperties);
    if (memory == nullptr) {
        device.destroyImage(image);
        printf("Image memory allocation failed\n");
        for (const auto& imageData : allocatedImageData) delete imageData;
        return nullptr;
    }

    memory->bindImage(image);

    ImageCube* returnImage = new ImageCube(imageCubeConfiguration.device, image, memory, size, imageCreateInfo.format);

    if (suppliedFaceData) {
        for (size_t i = 0; i < 6; ++i) {
            ImageRegion uploadRegion;
            uploadRegion.width = size;
            uploadRegion.height = size;
            ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
            if (!returnImage->upload((ImageCubeFace)i, cubeFacesImageData[i]->getData(), cubeFacesImageData[i]->getPixelLayout(), cubeFacesImageData[i]->getPixelFormat(), vk::ImageAspectFlagBits::eColor, uploadRegion, dstState)) {
                printf("Failed to upload buffer data\n");
                for (const auto& imageData : allocatedImageData) delete imageData;
                delete returnImage;
                return nullptr;
            }
        }
    }

    return returnImage;
}

bool ImageCube::upload(ImageCube* dstImage, const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
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

    if (!validateImageRegion(dstImage, face, imageRegion)) {
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

bool ImageCube::upload(const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
    return ImageCube::upload(this, face, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

std::shared_ptr<vkr::Device> ImageCube::getDevice() const {
    return m_device;
}

const vk::Image& ImageCube::getImage() const {
    return m_image;
}

const uint32_t& ImageCube::getSize() const {
    return m_size;
}

const uint32_t& ImageCube::getWidth() const {
    return m_size;
}

const uint32_t& ImageCube::getHeight() const {
    return m_size;
}

vk::Format ImageCube::getFormat() const {
    return m_format;
}

const GraphicsResource& ImageCube::getResourceId() const {
    return m_ResourceId;
}

std::array<bool, 6> ImageCube::loadCubeFacesImageData(const std::array<std::string, 6>& cubeFaceFilePaths, const vk::Format& format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData) {
    std::array<bool, 6> loadedImages = { false };

    ImagePixelLayout pixelLayout;
    ImagePixelFormat pixelFormat;
    bool hasPixelLayoutAndFormat = false;

    for (size_t i = 0; i < 6; ++i) {
        if (outImageData[i] != nullptr) {
            loadedImages[i] = true; // Already loaded
            continue;
        }

        if (cubeFaceFilePaths[i].empty()) {
            continue;
        }

        if (!hasPixelLayoutAndFormat) {
            hasPixelLayoutAndFormat = true;
            if (!ImageData::getPixelLayoutAndFormat(format, pixelLayout, pixelFormat)) {
                printf("Unable to create image: supplied image format %s has no corresponding loadable pixel format or layout\n", vk::to_string(format).c_str());
                return { false };
            }
        }

        ImageData* imageData = ImageData::load(cubeFaceFilePaths[i], pixelLayout, pixelFormat);
        if (imageData == nullptr) {
            printf("Failed to load image data for CubeImage face %llu from file \"%s\"\n", i, cubeFaceFilePaths[i].c_str());
            break;
        }

        delete outImageData[i];
        outImageData[i] = imageData;
        allocatedImageData.emplace_back(imageData);
        loadedImages[i] = true;
    }

    return loadedImages;
}

bool ImageCube::validateImageRegion(const ImageCube* image, const ImageCubeFace& face, ImageRegion& imageRegion) {
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
    imageRegion.baseLayer = (size_t)face;
    imageRegion.layerCount = 1;

    if (imageRegion.x + imageRegion.width > image->getWidth() || imageRegion.y + imageRegion.height > image->getHeight()) {
        printf("Image region out of range\n");
        return false;
    }

    return true;
}



void ImageViewCubeConfiguration::setImage(const vk::Image& image) {
    this->image = image;
}

void ImageViewCubeConfiguration::setImage(const ImageCube* image) {
    this->image = image->getImage();
}

ImageViewCube::ImageViewCube(std::weak_ptr<vkr::Device> device, vk::ImageView imageView):
    m_device(device),
    m_imageView(imageView),
    m_resourceId(GraphicsManager::nextResourceId()) {
}

ImageViewCube::~ImageViewCube() {
    (**m_device).destroyImageView(m_imageView);
}

ImageViewCube* ImageViewCube::create(const ImageViewCubeConfiguration& imageViewCubeConfiguration) {
    const vk::Device& device = **imageViewCubeConfiguration.device.lock();

    if (!imageViewCubeConfiguration.image) {
        printf("Unable to create ImageViewCube: Image is NULL\n");
        return nullptr;
    }

    vk::ImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.setImage(imageViewCubeConfiguration.image);
    imageViewCreateInfo.setViewType(vk::ImageViewType::eCube);
    imageViewCreateInfo.setFormat(imageViewCubeConfiguration.format);
    imageViewCreateInfo.components.setR(imageViewCubeConfiguration.redSwizzle);
    imageViewCreateInfo.components.setG(imageViewCubeConfiguration.greenSwizzle);
    imageViewCreateInfo.components.setB(imageViewCubeConfiguration.blueSwizzle);
    imageViewCreateInfo.components.setA(imageViewCubeConfiguration.alphaSwizzle);
    imageViewCreateInfo.subresourceRange.setAspectMask(imageViewCubeConfiguration.aspectMask);
    imageViewCreateInfo.subresourceRange.setBaseMipLevel(imageViewCubeConfiguration.baseMipLevel);
    imageViewCreateInfo.subresourceRange.setLevelCount(imageViewCubeConfiguration.mipLevelCount);
    imageViewCreateInfo.subresourceRange.setBaseArrayLayer(imageViewCubeConfiguration.baseArrayLayer * 6);
    imageViewCreateInfo.subresourceRange.setLayerCount(imageViewCubeConfiguration.arrayLayerCount * 6);

    vk::ImageView imageView = VK_NULL_HANDLE;

    vk::Result result;
    result = device.createImageView(&imageViewCreateInfo, nullptr, &imageView);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create ImageViewCube: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    return new ImageViewCube(imageViewCubeConfiguration.device, imageView);
}

std::shared_ptr<vkr::Device> ImageViewCube::getDevice() const {
    return m_device;
}

const vk::ImageView& ImageViewCube::getImageView() const {
    return m_imageView;
}

const GraphicsResource& ImageViewCube::getResourceId() const {
    return m_resourceId;
}