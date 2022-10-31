
#include "core/graphics/ImageCube.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/BufferView.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/application/Engine.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/util/Util.h"


ComputePipeline* ImageCube::s_computeEquirectangularPipeline = nullptr;
DescriptorSet* ImageCube::s_computeEquirectangularDescriptorSet = nullptr;

struct EquirectangularComputeUBO {
    glm::ivec2 faceSize;
    glm::ivec2 sourceSize;
    glm::ivec2 sampleCount;
};


void ImageSource::setImageData(ImageData* p_imageData, ImageData::ImageTransform* p_imageTransform) {
    imageData = p_imageData;
    filePath = "";
    setImageTransform(p_imageTransform);
}

void ImageSource::setFilePath(const std::string& p_filePath, ImageData::ImageTransform* p_imageTransform) {
    filePath = p_filePath;
    imageData = nullptr;
    setImageTransform(p_imageTransform);
}

void ImageSource::setImageTransform(ImageData::ImageTransform* p_imageTransform) {
    imageTransform = p_imageTransform;
}

bool ImageSource::hasSource() const {
    return imageData != nullptr || !filePath.empty();
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, const ImageSource& imageSource) {
    faceImages[face] = imageSource;
    equirectangularImage = {};
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, ImageData* imageData, ImageData::ImageTransform* imageTransform) {
    faceImages[face].setImageData(imageData, imageTransform);
    equirectangularImage = {};
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, const std::string& filePath, ImageData::ImageTransform* imageTransform) {
    faceImages[face].setFilePath(filePath, imageTransform);
    equirectangularImage = {};
}

void ImageCubeSource::setEquirectangularSource(const ImageSource& imageSource) {
    equirectangularImage = imageSource;
    faceImages = {};
}

void ImageCubeSource::setEquirectangularSource(ImageData* imageData, ImageData::ImageTransform* imageTransform) {
    equirectangularImage.setImageData(imageData, imageTransform);
    faceImages = {};
}

void ImageCubeSource::setEquirectangularSource(const std::string& filePath, ImageData::ImageTransform* imageTransform) {
    equirectangularImage.setFilePath(filePath, imageTransform);
    faceImages = {};
}

bool ImageCubeSource::isEquirectangular() const {
    return equirectangularImage.imageData != nullptr || !equirectangularImage.filePath.empty();
}


ImageCube::ImageCube(const WeakResource<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& size, const uint32_t& mipLevelCount, const vk::Format& format, const std::string& name):
        GraphicsResource(ResourceType_ImageCube, device, name),
        m_image(image),
        m_memory(memory),
        m_size(size),
        m_mipLevelCount(mipLevelCount),
        m_format(format) {
}

ImageCube::~ImageCube() {
    (**m_device).destroyImage(m_image);
    vfree(m_memory);
}

ImageCube* ImageCube::create(const ImageCubeConfiguration& imageCubeConfiguration, const std::string& name) {
    const vk::Device& device = **imageCubeConfiguration.device.lock(name);

    vk::Result result;
    uint32_t size;
    vk::ImageUsageFlags usage = imageCubeConfiguration.usage;

    bool suppliedFaceData = false;
    std::array<ImageData*, 6> cubeFacesImageData = {nullptr};

    bool suppliedEquirectangularData = false;
    ImageData* equirectangularImageData = nullptr;

    std::vector<ImageData*> allocatedImageData;

    if (imageCubeConfiguration.imageSource.isEquirectangular()) {
        bool loaded = loadImageData(imageCubeConfiguration.imageSource.equirectangularImage, imageCubeConfiguration.format, equirectangularImageData, allocatedImageData);
        suppliedEquirectangularData = loaded;

        if (!loaded && imageCubeConfiguration.imageSource.equirectangularImage.hasSource()) {
            printf("Unable to create CubeImage: Failed to load equirectangular image from supplied file paths\n");
            for (const auto& imageData : allocatedImageData) delete imageData;
            return nullptr;
        }

        // supplied size of 0 means automatically determine best size. For equirectangular
        // images, it will be half the height of the source image in most cases.
        size = imageCubeConfiguration.size == 0
               ? glm::max(1u, equirectangularImageData->getHeight() / 2)
               : imageCubeConfiguration.size;

    } else {
        std::array<bool, 6> loadedFaces = loadCubeFacesImageData(imageCubeConfiguration.imageSource.faceImages, imageCubeConfiguration.format, cubeFacesImageData, allocatedImageData);

        suppliedFaceData = loadedFaces[0];

        for (size_t i = 0; i < 6; ++i) {
            if ((suppliedFaceData && !loadedFaces[i]) ||  // Either all faces must be loaded or unloaded. Cannot have some and not others.
                (!loadedFaces[i] && imageCubeConfiguration.imageSource.faceImages[i].hasSource())) { // Check sources supplied were successfully loaded.

                printf("Unable to create CubeImage: Failed to load all faces from supplied file paths\n");
                for (const auto& imageData : allocatedImageData) delete imageData;
                return nullptr;
            }
        }


        if (suppliedFaceData) {
            size = cubeFacesImageData[0]->getWidth();

            for (size_t i = 0; i < 6; ++i) {
                if (cubeFacesImageData[i]->getWidth() != cubeFacesImageData[i]->getHeight()) {
                    printf("Unable to load CubeImage: Supplied image data is not square. Width must equal height for all faces\n");
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
            if (imageCubeConfiguration.imageSource.faceImages[i].imageTransform != nullptr &&
                !imageCubeConfiguration.imageSource.faceImages[i].imageTransform->isNoOp()) {
                ImageData* transformedImageData = ImageData::transform(cubeFacesImageData[i], *imageCubeConfiguration.imageSource.faceImages[i].imageTransform);
                // The previous image data remains in the allocatedImageData array and will be deleted later.
                cubeFacesImageData[i] = transformedImageData;
                allocatedImageData.emplace_back(transformedImageData);
            }
        }
    }

    if (suppliedFaceData || suppliedEquirectangularData) {
        usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    bool generateMipmap = imageCubeConfiguration.generateMipmap && imageCubeConfiguration.mipLevels > 1;
    if (generateMipmap) {
        usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
    }

    uint32_t maxMipLevels = ImageUtil::getMaxMipLevels(size, size, 1);
    uint32_t mipLevels = glm::clamp(imageCubeConfiguration.mipLevels, 1u, maxMipLevels);

    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
    imageCreateInfo.setImageType(vk::ImageType::e2D);
    imageCreateInfo.setFormat(imageCubeConfiguration.format);
    imageCreateInfo.extent.setWidth(size);
    imageCreateInfo.extent.setHeight(size);
    imageCreateInfo.extent.setDepth(1);
    imageCreateInfo.setMipLevels(mipLevels);
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

    Engine::graphics()->setObjectName(device, (uint64_t)(VkImage)image, vk::ObjectType::eImage, name);

    const vk::MemoryRequirements& memoryRequirements = device.getImageMemoryRequirements(image);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, imageCubeConfiguration.memoryProperties);
    if (memory == nullptr) {
        device.destroyImage(image);
        printf("Image memory allocation failed\n");
        for (const auto& imageData : allocatedImageData) delete imageData;
        return nullptr;
    }

    memory->bindImage(image);

    ImageCube* returnImage = new ImageCube(imageCubeConfiguration.device, image, memory, size, mipLevels, imageCreateInfo.format, name);

    if (suppliedEquirectangularData) {
        ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
        ImageRegion region;
        region.width = equirectangularImageData->getWidth();
        region.height = equirectangularImageData->getHeight();

        printf("Uploading ImageCube equirectangular data [%d x %d] with face size [%d x %d]\n", (int32_t)region.width, (int32_t)region.height, (int32_t)size, (int32_t)size);

        if (!returnImage->uploadEquirectangular(equirectangularImageData->getData(), equirectangularImageData->getPixelLayout(), equirectangularImageData->getPixelFormat(), vk::ImageAspectFlagBits::eColor, region, dstState)) {
            printf("Failed to upload buffer data\n");
            delete returnImage;
            returnImage = nullptr;
        }

        if (generateMipmap) {
            auto t0 = Performance::now();
            returnImage->generateMipmap(vk::Filter::eLinear, vk::ImageAspectFlagBits::eColor, mipLevels, dstState);
            printf("Took %.2f msec to generate %u mipmap levels for ImageCube\n", Performance::milliseconds(t0), mipLevels);
        }

    } else if (suppliedFaceData) {
        ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
        ImageRegion region;
        region.width = size;
        region.height = size;

        printf("Uploading ImageCube face data with face size [%d x %d]\n", (int32_t)size, (int32_t)size);

        for (size_t i = 0; i < 6; ++i) {
            if (!returnImage->uploadFace((ImageCubeFace)i, cubeFacesImageData[i]->getData(), cubeFacesImageData[i]->getPixelLayout(), cubeFacesImageData[i]->getPixelFormat(), vk::ImageAspectFlagBits::eColor, region, dstState)) {
                printf("Failed to upload buffer data\n");
                delete returnImage;
                returnImage = nullptr;
            }
        }

        if (generateMipmap) {
            auto t0 = Performance::now();
            returnImage->generateMipmap(vk::Filter::eLinear, vk::ImageAspectFlagBits::eColor, mipLevels, dstState);
            printf("Took %.2f msec to generate %u mipmap levels for ImageCube\n", Performance::milliseconds(t0), mipLevels);
        }
    } else if (generateMipmap) {
        printf("GenerateMipmap requested for ImageCube \"%s\", but no source data was uploaded to generate from\n", name.c_str());
    }

    for (const auto& imageData : allocatedImageData) delete imageData;
    return returnImage;
}

bool ImageCube::uploadFace(ImageCube* dstImage, const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
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

    if (!validateFaceImageRegion(dstImage, face, imageRegion)) {
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

bool ImageCube::uploadEquirectangular(ImageCube* dstImage, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
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

    if (!validateEquirectangularImageRegion(dstImage, imageRegion)) {
        return false;
    }

    uint32_t mipLevel = glm::min((uint32_t)imageRegion.baseMipLevel, dstImage->getMipLevelCount());
    const ImageRegion::size_type& equirectangularWidth = imageRegion.width;
    const ImageRegion::size_type& equirectangularHeight = imageRegion.height;
    uint32_t cubeImageWidth = glm::max(1u, dstImage->getWidth() >> mipLevel);
    uint32_t cubeImageHeight = glm::max(1u, dstImage->getWidth() >> mipLevel);

    ImageData* tempImageData = nullptr;

    if (dstPixelFormat != pixelFormat || dstPixelLayout != pixelLayout) {
        tempImageData = ImageData::mutate(static_cast<uint8_t*>(data), equirectangularWidth, equirectangularHeight, pixelLayout, pixelFormat, dstPixelLayout, dstPixelFormat);
    } else {
        tempImageData = new ImageData(static_cast<uint8_t*>(data), equirectangularWidth, equirectangularHeight, pixelLayout, pixelFormat);
    }

    uint32_t bytesPerPixel = ImageData::getChannelSize(dstPixelFormat) * ImageData::getChannels(dstPixelLayout);

    if (bytesPerPixel == 0) {
        printf("Unable to upload image data: Invalid image pixel format\n");
        delete tempImageData;
        return false;
    }

    bool success = false;

    const vk::Device& device = **Engine::graphics()->getDevice();

    EquirectangularComputeUBO uniformBufferData{};

    uniformBufferData.faceSize.x = (int32_t)dstImage->getSize();
    uniformBufferData.faceSize.y = (int32_t)dstImage->getSize();
    uniformBufferData.sourceSize.x = (int32_t)equirectangularWidth;
    uniformBufferData.sourceSize.y = (int32_t)equirectangularHeight;
    uniformBufferData.sampleCount = glm::ivec2(8);

    BufferConfiguration tempBufferConfig{};
    tempBufferConfig.device = Engine::graphics()->getDevice();
    tempBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
//    tempBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    tempBufferConfig.usage = vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc;

    vk::DeviceSize equirectangularImageSizeBytes = (vk::DeviceSize)equirectangularWidth * (vk::DeviceSize)equirectangularHeight * (vk::DeviceSize)bytesPerPixel;
    vk::DeviceSize cubemapImageSizeBytes = (vk::DeviceSize)cubeImageWidth * (vk::DeviceSize)cubeImageHeight * 6 * (vk::DeviceSize)bytesPerPixel;
    vk::DeviceSize equirectangularBufferOffsetBytes = INT_DIV_CEIL(sizeof(EquirectangularComputeUBO), 256) * 256;
    vk::DeviceSize cubemapBufferOffsetBytes = INT_DIV_CEIL(equirectangularBufferOffsetBytes + equirectangularImageSizeBytes, 256) * 256;
    tempBufferConfig.size = INT_DIV_CEIL(cubemapBufferOffsetBytes + cubemapImageSizeBytes, 256) * 256;

    Buffer* tempBuffer = Buffer::create(tempBufferConfig, "ImageCube-EquirectangularComputeUniformBuffer");

    if (tempBuffer == nullptr) {
        printf("Unable to upload equirectangular CubeMap image data: Failed to create staging buffer\n");
        delete tempImageData;
        return false;
    }

    tempBuffer->upload(0, sizeof(EquirectangularComputeUBO), &uniformBufferData);
    tempBuffer->upload(equirectangularBufferOffsetBytes, equirectangularImageSizeBytes, tempImageData->getData());
    delete tempImageData;

    if (tempBuffer == nullptr) {
        printf("Failed to create image data staging buffer\n");
        return false;
    }

    BufferViewConfiguration imageBufferViewConfig{};
    imageBufferViewConfig.device = Engine::graphics()->getDevice();
    imageBufferViewConfig.setBuffer(tempBuffer);
    imageBufferViewConfig.setFormat(dstImage->getFormat());

    imageBufferViewConfig.setOffsetRange(cubemapBufferOffsetBytes, cubemapImageSizeBytes);
    BufferView* cubemapImageBufferView = BufferView::create(imageBufferViewConfig, "ImageCube-CubemapImageBufferView");
    if (cubemapImageBufferView == nullptr) {
        printf("Unable to upload cube map image data: Failed to create texel staging buffer\n");
        delete tempBuffer;
        return false;
    }

    imageBufferViewConfig.setOffsetRange(equirectangularBufferOffsetBytes, equirectangularImageSizeBytes);
    BufferView* equirectangularImageBufferView = BufferView::create(imageBufferViewConfig, "ImageCube-EquirectangularImageBufferView");
    if (equirectangularImageBufferView == nullptr) {
        printf("Unable to upload cube map image data: Failed to create texel staging buffer\n");
        delete cubemapImageBufferView;
        delete tempBuffer;
        return false;
    }

    const vk::Queue& computeQueue = ImageUtil::getComputeQueue();
    const vk::CommandBuffer& commandBuffer = ImageUtil::getComputeCommandBuffer();
    ComputePipeline* equirectangularComputePipeline = getEquirectangularComputePipeline();
    DescriptorSet* equirectangularComputeDescriptorSet = getEquirectangularComputeDescriptorSet();

    vk::CommandBufferBeginInfo commandBeginInfo{};
    commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(commandBeginInfo);
    PROFILE_BEGIN_GPU_CMD("ImageCube::uploadEquirectangular/ComputeCubeMap", commandBuffer);


    DescriptorSetWriter(equirectangularComputeDescriptorSet)
            .writeBuffer(0, tempBuffer, 0, sizeof(EquirectangularComputeUBO))
            .writeTexelBufferView(1, equirectangularImageBufferView)
            .writeTexelBufferView(2, cubemapImageBufferView)
            .write();

    std::array<vk::DescriptorSet, 1> descriptorSets = {equirectangularComputeDescriptorSet->getDescriptorSet()};

    equirectangularComputePipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, equirectangularComputePipeline->getPipelineLayout(), 0, descriptorSets, nullptr);
    equirectangularComputePipeline->dispatch(commandBuffer, (uint32_t)glm::ceil(cubeImageWidth / 16), (uint32_t)glm::ceil(cubeImageHeight / 16), 6);

    PROFILE_END_GPU_CMD(commandBuffer);
    commandBuffer.end();

    vk::SubmitInfo queueSubmitInfo;
    queueSubmitInfo.setCommandBufferCount(1);
    queueSubmitInfo.setPCommandBuffers(&commandBuffer);
    vk::Result result = computeQueue.submit(1, &queueSubmitInfo, VK_NULL_HANDLE);
    assert(result == vk::Result::eSuccess);
    computeQueue.waitIdle();

    vk::BufferImageCopy imageCopy{};
    imageCopy.setBufferOffset(cubemapBufferOffsetBytes);
    imageCopy.setBufferRowLength(0);
    imageCopy.setBufferImageHeight(0);
    imageCopy.imageSubresource.setAspectMask(aspectMask);
    imageCopy.imageSubresource.setMipLevel(imageRegion.baseMipLevel);
    imageCopy.imageSubresource.setBaseArrayLayer(imageRegion.baseLayer);
    imageCopy.imageSubresource.setLayerCount(imageRegion.layerCount * 6);
    imageCopy.imageOffset.setX(0);
    imageCopy.imageOffset.setY(0);
    imageCopy.imageOffset.setZ(0);
    imageCopy.imageExtent.setWidth(cubeImageWidth);
    imageCopy.imageExtent.setHeight(cubeImageHeight);
    imageCopy.imageExtent.setDepth(1);

    const vk::CommandBuffer& transferCommandBuffer = ImageUtil::beginTransferCommands();

    success = ImageUtil::transferBuffer(transferCommandBuffer, dstImage->getImage(), tempBuffer->getBuffer(), imageCopy, dstState);

    if (dstImage->getMipLevelCount() > 1) {
        // Transition all other mip levels to dstState, since transferBuffer only transitions imageCopy.imageSubresource.mipLevel
        vk::ImageSubresourceRange subresourceRange{};
        subresourceRange.setAspectMask(imageCopy.imageSubresource.aspectMask);
        subresourceRange.setBaseArrayLayer(imageCopy.imageSubresource.baseArrayLayer);
        subresourceRange.setLayerCount(imageCopy.imageSubresource.layerCount);
        subresourceRange.setBaseMipLevel(0);
        subresourceRange.setLevelCount(dstImage->getMipLevelCount());
        ImageUtil::transitionLayout(transferCommandBuffer, dstImage->getImage(), subresourceRange, ImageTransition::FromAny(), dstState);
    }

    ImageUtil::endTransferCommands(**Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN), true);

    if (!success)
        printf("Failed to transfer buffer data to destination CubeMap image\n");

    delete equirectangularImageBufferView;
    delete cubemapImageBufferView;
    delete tempBuffer;
    return success;
}

bool ImageCube::uploadFace(const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
    return ImageCube::uploadFace(this, face, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

bool ImageCube::uploadEquirectangular(void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState) {
    return ImageCube::uploadEquirectangular(this, data, pixelLayout, pixelFormat, aspectMask, imageRegion, dstState);
}

bool ImageCube::generateMipmap(ImageCube* image, const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState) {
    return ImageUtil::generateMipmap(image->getImage(), image->getFormat(), filter, aspectMask, 0, 6, image->getWidth(), image->getHeight(), 1, mipLevels, dstState);
}

bool ImageCube::generateMipmap(const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState) {
    return ImageCube::generateMipmap(this, filter, aspectMask, mipLevels, dstState);
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

const uint32_t ImageCube::getMipLevelCount() const {
    return m_mipLevelCount;
}

vk::Format ImageCube::getFormat() const {
    return m_format;
}

std::array<bool, 6> ImageCube::loadCubeFacesImageData(const std::array<ImageSource, 6>& cubeFaceImageSources, const vk::Format& format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData) {
    std::array<bool, 6> loadedImages = {false};

    for (size_t i = 0; i < 6; ++i)
        loadedImages[i] = loadImageData(cubeFaceImageSources[i], format, outImageData[i], allocatedImageData);

    return loadedImages;
}

bool ImageCube::loadImageData(const ImageSource& imageSource, const vk::Format& format, ImageData*& outImageData, std::vector<ImageData*>& allocatedImageData) {
    if (outImageData != nullptr)
        return false; // Image was not loaded, we will not delete/overwrite the already existing ImageData from here. This is probably an error.

    if (imageSource.imageData != nullptr) {
        outImageData = imageSource.imageData;
        return true;
    }

    if (imageSource.filePath.empty()) {
        return false;
    }

    ImagePixelLayout pixelLayout;
    ImagePixelFormat pixelFormat;
    if (!ImageData::getPixelLayoutAndFormat(format, pixelLayout, pixelFormat)) {
        printf("Unable to create image: supplied image format %s has no corresponding loadable pixel format or layout\n", vk::to_string(format).c_str());
        return false;
    }

    ImageData* imageData = ImageData::load(imageSource.filePath, pixelLayout, pixelFormat);
    if (imageData == nullptr) {
        printf("Failed to load image data from file \"%s\"\n", imageSource.filePath.c_str());
        return false;
    }

    allocatedImageData.emplace_back(imageData);
    outImageData = imageData;
    return true;
}

bool ImageCube::validateFaceImageRegion(const ImageCube* image, const ImageCubeFace& face, ImageRegion& imageRegion) {
    if (image == nullptr) {
        return false;
    }

    if (imageRegion.x >= image->getWidth() || imageRegion.y >= image->getHeight()) {
        printf("Image region out of range\n");
        return false;
    }

    if (imageRegion.width == ImageRegion::WHOLE_SIZE) imageRegion.width = image->getWidth() - imageRegion.x;
    if (imageRegion.height == ImageRegion::WHOLE_SIZE) imageRegion.height = image->getHeight() - imageRegion.y;
    if (imageRegion.mipLevelCount == ImageRegion::WHOLE_SIZE) imageRegion.mipLevelCount = 1;
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

bool ImageCube::validateEquirectangularImageRegion(const ImageCube* image, ImageRegion& imageRegion) {
    if (image == nullptr) {
        return false;
    }

    if (imageRegion.width == ImageRegion::WHOLE_SIZE || imageRegion.height == ImageRegion::WHOLE_SIZE || imageRegion.width == 0 || imageRegion.height == 0) {
        printf("Equirectangular image region must have width and height supplied\n");
        return false;
    }

//    imageRegion.baseMipLevel = 0;
    if (imageRegion.mipLevelCount == ImageRegion::WHOLE_SIZE) imageRegion.mipLevelCount = image->getMipLevelCount() - imageRegion.baseMipLevel;
    imageRegion.x = 0;
    imageRegion.y = 0;
    imageRegion.z = 0;
    imageRegion.depth = 1;
    imageRegion.baseLayer = 0;
    imageRegion.layerCount = 1;

    return true;
}


ComputePipeline* ImageCube::getEquirectangularComputePipeline() {
    if (s_computeEquirectangularPipeline == nullptr) {

        ComputePipelineConfiguration pipelineConfig{};
        pipelineConfig.device = Engine::graphics()->getDevice();
        pipelineConfig.computeShader = "res/shaders/compute/compute_equirectangular.glsl";
        pipelineConfig.addDescriptorSetLayout(getEquirectangularComputeDescriptorSet()->getLayout().get());
        s_computeEquirectangularPipeline = ComputePipeline::create(pipelineConfig, "ComputeEquirectangularPipeline");

        Engine::eventDispatcher()->connect(&ImageCube::onCleanupGraphics);
    }

    return s_computeEquirectangularPipeline;
}

DescriptorSet* ImageCube::getEquirectangularComputeDescriptorSet() {

    if (s_computeEquirectangularDescriptorSet == nullptr) {
        SharedResource<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(Engine::graphics()->getDevice())
                .addUniformBuffer(0, vk::ShaderStageFlagBits::eCompute)
                .addStorageTexelBuffer(1, vk::ShaderStageFlagBits::eCompute)
                .addStorageTexelBuffer(2, vk::ShaderStageFlagBits::eCompute)
                .build("ImageCube-EquirectangularComputeDescriptorSetLayout");

        s_computeEquirectangularDescriptorSet = DescriptorSet::create(descriptorSetLayout, Engine::graphics()->descriptorPool(), "ImageCube-EquirectangularComputeDescriptorSet");

        Engine::eventDispatcher()->connect(&ImageCube::onCleanupGraphics);
    }

    return s_computeEquirectangularDescriptorSet;
}

void ImageCube::onCleanupGraphics(ShutdownGraphicsEvent* event) {
    printf("Destroying ImageCube equirectangular compute resources\n");
    delete s_computeEquirectangularDescriptorSet;
    delete s_computeEquirectangularPipeline;
}

