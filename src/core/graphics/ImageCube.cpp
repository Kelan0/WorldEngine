
#include "core/graphics/ImageCube.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/BufferView.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/ComputePipeline.h"
#include "core/graphics/DescriptorSet.h"
#include "core/application/Application.h"
#include "core/util/Util.h"



ComputePipeline* ImageCube::s_computeEquirectangularPipeline = nullptr;
DescriptorSet* ImageCube::s_computeEquirectangularDescriptorSet = nullptr;

struct EquirectangularComputeUBO {
    glm::ivec2 faceSize;
    glm::ivec2 sourceSize;
};



void ImageSource::setImageData(ImageData* imageData, ImageData::ImageTransform* imageTransform) {
    this->imageData = imageData;
    this->filePath = "";
    setImageTransform(imageTransform);
}

void ImageSource::setFilePath(const std::string& filePath, ImageData::ImageTransform* imageTransform) {
    this->filePath = filePath;
    this->imageData = nullptr;
    setImageTransform(imageTransform);
}

void ImageSource::setImageTransform(ImageData::ImageTransform* imageTransform) {
    this->imageTransform = imageTransform;
}

bool ImageSource::hasSource() const {
    return this->imageData != nullptr || !this->filePath.empty();
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, const ImageSource& imageSource) {
    this->faceImages[face] = imageSource;
    this->equirectangularImage = {};
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, ImageData* imageData, ImageData::ImageTransform* imageTransform) {
    this->faceImages[face].setImageData(imageData, imageTransform);
    this->equirectangularImage = {};
}

void ImageCubeSource::setFaceSource(const ImageCubeFace& face, const std::string& filePath, ImageData::ImageTransform* imageTransform) {
    this->faceImages[face].setFilePath(filePath, imageTransform);
    this->equirectangularImage = {};
}

void ImageCubeSource::setEquirectangularSource(const ImageSource& imageSource) {
    this->equirectangularImage = imageSource;
    this->faceImages = {};
}

void ImageCubeSource::setEquirectangularSource(ImageData* imageData, ImageData::ImageTransform* imageTransform) {
    this->equirectangularImage.setImageData(imageData, imageTransform);
    this->faceImages = {};
}

void ImageCubeSource::setEquirectangularSource(const std::string& filePath, ImageData::ImageTransform* imageTransform) {
    this->equirectangularImage.setFilePath(filePath, imageTransform);
    this->faceImages = {};
}

bool ImageCubeSource::isEquirectangular() const {
    return this->equirectangularImage.imageData != nullptr || this->equirectangularImage.filePath != "";
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
    uint32_t size;
    vk::ImageUsageFlags usage = imageCubeConfiguration.usage;

    bool suppliedFaceData = false;
    std::array<ImageData*, 6> cubeFacesImageData = { nullptr };

    bool suppliedEquirectangularData = false;
    ImageData* equirectangularImageData = nullptr;

    std::vector<ImageData*> allocatedImageData;

    if (imageCubeConfiguration.imageSource.isEquirectangular()) {
        bool loaded = loadImageData(imageCubeConfiguration.imageSource.equirectangularImage, imageCubeConfiguration.format, equirectangularImageData, allocatedImageData);
        suppliedEquirectangularData = loaded;

        if (!loaded && imageCubeConfiguration.imageSource.equirectangularImage.hasSource()) {
            printf("Unable to create CubeImage: Failed to load equirectangular image from supplied file paths\n");
            for (const auto &imageData: allocatedImageData) delete imageData;
            return nullptr;
        }

        // supplied size of 0 means automatically determine best size. For equirectangular
        // images, it will be half the height of the source image in most cases.
        size = imageCubeConfiguration.size == 0
                ? equirectangularImageData->getHeight() / 2
                : imageCubeConfiguration.size;

    } else {
        std::array<bool, 6> loadedFaces = loadCubeFacesImageData(imageCubeConfiguration.imageSource.faceImages, imageCubeConfiguration.format, cubeFacesImageData, allocatedImageData);

        suppliedFaceData = loadedFaces[0];

        for (size_t i = 0; i < 6; ++i) {
            if (
                    (suppliedFaceData && !loadedFaces[i]) ||  // Either all faces must be loaded or unloaded. Cannot have some and not others.
                    (!loadedFaces[i] && imageCubeConfiguration.imageSource.faceImages[i].hasSource()) // Check sources supplied were successfully loaded.
            ) {
                printf("Unable to create CubeImage: Failed to load all faces from supplied file paths\n");
                for (const auto &imageData: allocatedImageData) delete imageData;
                return nullptr;
            }
        }


        if (suppliedFaceData) {
            size = cubeFacesImageData[0]->getWidth();

            for (size_t i = 0; i < 6; ++i) {
                if (cubeFacesImageData[i]->getWidth() != cubeFacesImageData[i]->getHeight()) {
                    printf("Unable to load CubeImage: Supplied image data is now square. Width must equal height for all faces\n");
                    for (const auto &imageData: allocatedImageData) delete imageData;
                    return nullptr;
                }

                if (cubeFacesImageData[i]->getWidth() != size) {
                    printf("Unable to load CubeImage: Supplied image data has mismatched sizes. All faces must have the same size\n");
                    for (const auto &imageData: allocatedImageData) delete imageData;
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

    if (suppliedFaceData || suppliedEquirectangularData)
        usage |= vk::ImageUsageFlagBits::eTransferDst;

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

    if (suppliedEquirectangularData) {
        ImageRegion region;
        region.width = equirectangularImageData->getWidth();
        region.height = equirectangularImageData->getHeight();

        ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
        if (!returnImage->uploadEquirectangular(equirectangularImageData->getData(), equirectangularImageData->getPixelLayout(), equirectangularImageData->getPixelFormat(), vk::ImageAspectFlagBits::eColor, region, dstState)) {
            printf("Failed to upload buffer data\n");
            delete returnImage;
            returnImage = nullptr;
        }

    } else if (suppliedFaceData) {
        for (size_t i = 0; i < 6; ++i) {
            ImageRegion region;
            region.width = size;
            region.height = size;
            ImageTransitionState dstState = ImageTransition::ShaderReadOnly(vk::PipelineStageFlagBits::eFragmentShader);
            if (!returnImage->uploadFace((ImageCubeFace) i, cubeFacesImageData[i]->getData(), cubeFacesImageData[i]->getPixelLayout(), cubeFacesImageData[i]->getPixelFormat(), vk::ImageAspectFlagBits::eColor, region, dstState)) {
                printf("Failed to upload buffer data\n");
                delete returnImage;
                returnImage = nullptr;
            }
        }
    }

    for (const auto &imageData: allocatedImageData) delete imageData;
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

    if (!validateEquirectangularFaceImageRegion(dstImage, imageRegion)) {
        return false;
    }

    ImageData* tempImageData = nullptr;

    if (dstPixelFormat != pixelFormat || dstPixelLayout != pixelLayout) {
        tempImageData = ImageData::mutate(static_cast<uint8_t*>(data), imageRegion.width, imageRegion.height, pixelLayout, pixelFormat, dstPixelLayout, dstPixelFormat);
    } else {
        tempImageData = new ImageData(static_cast<uint8_t*>(data), imageRegion.width, imageRegion.height, pixelLayout, pixelFormat);
    }

    uint32_t bytesPerPixel = ImageData::getChannelSize(dstPixelFormat) * ImageData::getChannels(dstPixelLayout);

    if (bytesPerPixel == 0) {
        printf("Unable to upload image data: Invalid image pixel format\n");
        delete tempImageData;
        return false;
    }

    bool success = false;

    const vk::Device& device = **Application::instance()->graphics()->getDevice();

    EquirectangularComputeUBO uniformBufferData;

    uniformBufferData.faceSize.x = dstImage->getSize();
    uniformBufferData.faceSize.y = dstImage->getSize();
    uniformBufferData.sourceSize.x = imageRegion.width;
    uniformBufferData.sourceSize.y = imageRegion.height;

    vk::DeviceSize cubemapImageSizeBytes = (vk::DeviceSize)dstImage->getWidth() * (vk::DeviceSize)dstImage->getHeight() * 6 * (vk::DeviceSize)bytesPerPixel;
    vk::DeviceSize equirectangularImageSizeBytes = (vk::DeviceSize)imageRegion.width * (vk::DeviceSize)imageRegion.height * (vk::DeviceSize)bytesPerPixel;
    vk::DeviceSize equirectangularBufferOffsetBytes = sizeof(EquirectangularComputeUBO);
    vk::DeviceSize cubemapBufferOffsetBytes = equirectangularBufferOffsetBytes + cubemapImageSizeBytes;

    BufferConfiguration tempBufferConfig;
    tempBufferConfig.device = Application::instance()->graphics()->getDevice();
    tempBufferConfig.size = cubemapImageSizeBytes + equirectangularImageSizeBytes + sizeof(EquirectangularComputeUBO);
    tempBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
//    tempBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    tempBufferConfig.usage = vk::BufferUsageFlagBits::eStorageTexelBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc;
    Buffer* tempBuffer = Buffer::create(tempBufferConfig);

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
    imageBufferViewConfig.device = Application::instance()->graphics()->getDevice();
    imageBufferViewConfig.setBuffer(tempBuffer);
    imageBufferViewConfig.setFormat(dstImage->getFormat());

    imageBufferViewConfig.setOffsetRange(cubemapBufferOffsetBytes, cubemapImageSizeBytes);
    BufferView* cubemapImageBufferView = BufferView::create(imageBufferViewConfig);
    if (cubemapImageBufferView == nullptr) {
        printf("Unable to upload cube map image data: Failed to create texel staging buffer\n");
        delete tempBuffer;
        return false;
    }

    imageBufferViewConfig.setOffsetRange(equirectangularBufferOffsetBytes, equirectangularImageSizeBytes);
    BufferView* equirectangularImageBufferView = BufferView::create(imageBufferViewConfig);
    if (equirectangularImageBufferView == nullptr) {
        printf("Unable to upload cube map image data: Failed to create texel staging buffer\n");
        delete cubemapImageBufferView;
        delete tempBuffer;
        return false;
    }

    const vk::Queue &computeQueue = **Application::instance()->graphics()->getQueue(QUEUE_COMPUTE_MAIN);
    const vk::CommandBuffer &commandBuffer = **Application::instance()->graphics()->commandPool()->getCommandBuffer("image_compute_buffer");
    ComputePipeline* equirectangularComputePipeline = getEquirectangularComputePipeline();
    DescriptorSet* equirectangularComputeDescriptorSet = getEquirectangularComputeDescriptorSet();

    vk::CommandBufferBeginInfo commandBeginInfo;
    commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(commandBeginInfo);


    DescriptorSetWriter(equirectangularComputeDescriptorSet)
            .writeBuffer(0, tempBuffer, 0, sizeof(EquirectangularComputeUBO))
            .writeTexelBufferView(1, equirectangularImageBufferView)
            .writeTexelBufferView(2, cubemapImageBufferView)
            .write();

    std::vector<vk::DescriptorSet> descriptorSets = { equirectangularComputeDescriptorSet->getDescriptorSet() };

    equirectangularComputePipeline->bind(commandBuffer);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,equirectangularComputePipeline->getPipelineLayout(), 0, descriptorSets,nullptr);
    equirectangularComputePipeline->dispatch(commandBuffer, (uint32_t) glm::ceil(dstImage->getWidth() / 16),(uint32_t) glm::ceil(dstImage->getHeight() / 16), 1);

    commandBuffer.end();

    vk::SubmitInfo queueSumbitInfo;
    queueSumbitInfo.setCommandBufferCount(1);
    queueSumbitInfo.setPCommandBuffers(&commandBuffer);
    computeQueue.submit(1, &queueSumbitInfo, VK_NULL_HANDLE);
    computeQueue.waitIdle();

    vk::BufferImageCopy imageCopy{};
    imageCopy.setBufferOffset(cubemapBufferOffsetBytes);
    imageCopy.setBufferRowLength(0);
    imageCopy.setBufferImageHeight(0);
    imageCopy.imageSubresource.setAspectMask(aspectMask);
    imageCopy.imageSubresource.setMipLevel(0);
    imageCopy.imageSubresource.setBaseArrayLayer(0);
    imageCopy.imageSubresource.setLayerCount(6);
    imageCopy.imageOffset.setX(0);
    imageCopy.imageOffset.setY(0);
    imageCopy.imageOffset.setZ(0);
    imageCopy.imageExtent.setWidth(dstImage->getWidth());
    imageCopy.imageExtent.setHeight(dstImage->getHeight());
    imageCopy.imageExtent.setDepth(1);

    success = ImageUtil::transferBuffer(dstImage->getImage(), tempBuffer->getBuffer(), imageCopy, aspectMask, 0, 6, 0, 1, dstState);

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

std::array<bool, 6> ImageCube::loadCubeFacesImageData(const std::array<ImageSource, 6>& cubeFaceImageSources, const vk::Format& format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData) {
    std::array<bool, 6> loadedImages = { false };

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

bool ImageCube::validateEquirectangularFaceImageRegion(const ImageCube* image, ImageRegion& imageRegion) {
    if (image == nullptr) {
        return false;
    }

    if (imageRegion.width == VK_WHOLE_SIZE || imageRegion.height == VK_WHOLE_SIZE || imageRegion.width == 0 || imageRegion.height == 0) {
        printf("Equirectangular image region must have width and height supplied\n");
        return false;
    }

    imageRegion.baseMipLevel = 0;
    if (imageRegion.mipLevelCount == VK_WHOLE_SIZE) imageRegion.mipLevelCount = 1;
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

        ComputePipelineConfiguration pipelineConfig;
        pipelineConfig.device = Application::instance()->graphics()->getDevice();
        pipelineConfig.computeShader = "res/shaders/compute/compute_equirectangular.glsl";
        pipelineConfig.addDescriptorSetLayout(getEquirectangularComputeDescriptorSet()->getLayout().get());
        s_computeEquirectangularPipeline = ComputePipeline::create(pipelineConfig);
    }

    return s_computeEquirectangularPipeline;
}

DescriptorSet* ImageCube::getEquirectangularComputeDescriptorSet() {

    if (s_computeEquirectangularDescriptorSet == nullptr) {
        std::shared_ptr<DescriptorSetLayout> descriptorSetLayout = DescriptorSetLayoutBuilder(
                Application::instance()->graphics()->getDevice())
                .addUniformBuffer(0, vk::ShaderStageFlagBits::eCompute, sizeof(EquirectangularComputeUBO))
                .addStorageTexelBuffer(1, vk::ShaderStageFlagBits::eCompute)
                .addStorageTexelBuffer(2, vk::ShaderStageFlagBits::eCompute)
                .build();

        s_computeEquirectangularDescriptorSet = DescriptorSet::create(descriptorSetLayout, Application::instance()->graphics()->descriptorPool());
    }

    return s_computeEquirectangularDescriptorSet;
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