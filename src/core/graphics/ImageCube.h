
#ifndef WORLDENGINE_IMAGECUBE_H
#define WORLDENGINE_IMAGECUBE_H

#include "core/core.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/GraphicsResource.h"

class DeviceMemoryBlock;

class ComputePipeline;

class DescriptorSet;

struct ShutdownGraphicsEvent;

enum ImageCubeFace : uint8_t {
    ImageCubeFace_PosX = 0,
    ImageCubeFace_NegX = 1,
    ImageCubeFace_PosY = 2,
    ImageCubeFace_NegY = 3,
    ImageCubeFace_PosZ = 4,
    ImageCubeFace_NegZ = 5,
};

struct ImageSource {
    ImageData* imageData = nullptr;
    std::string filePath;
    ImageData::ImageTransform* imageTransform = nullptr;

    void setImageData(ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);

    void setFilePath(const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);

    void setImageTransform(ImageData::ImageTransform* imageTransform);

    bool hasSource() const;
};

struct ImageCubeSource {
    std::array<ImageSource, 6> faceImages = {};
    ImageSource equirectangularImage = {};

    void setFaceSource(ImageCubeFace face, const ImageSource& imageSource);

    void setFaceSource(ImageCubeFace face, ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);

    void setFaceSource(ImageCubeFace face, const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);

    void setEquirectangularSource(const ImageSource& imageSource);

    void setEquirectangularSource(ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);

    void setEquirectangularSource(const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);

    bool isEquirectangular() const;
};

struct ImageCubeConfiguration {
    WeakResource<vkr::Device> device;
    ImageCubeSource imageSource = {};
    uint32_t size = 0;
    uint32_t mipLevels = 1;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    bool enabledTexelAccess = false;
    bool preInitialized = false;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    bool generateMipmap = false;
};

class ImageCube : public GraphicsResource {
    NO_COPY(ImageCube);
private:
    ImageCube(const WeakResource<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, uint32_t size, uint32_t mipLevelCount, vk::Format format, const std::string& name);

public:
    ~ImageCube() override;

    static ImageCube* create(const ImageCubeConfiguration& imageCubeConfiguration, const std::string& name);

    static bool uploadFace(ImageCube* dstImage, ImageCubeFace face, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool uploadEquirectangular(ImageCube* dstImage, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool uploadFace(ImageCubeFace face, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool uploadEquirectangular(void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool generateMipmap(ImageCube* image, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& dstState);

    bool generateMipmap(vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& dstState);

    const vk::Image& getImage() const;

    uint32_t getSize() const;

    uint32_t getWidth() const;

    uint32_t getHeight() const;

    const uint32_t getMipLevelCount() const;

    vk::Format getFormat() const;

    static std::array<bool, 6> loadCubeFacesImageData(const std::array<ImageSource, 6>& cubeFaceImageSources, vk::Format format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData);

    static bool loadImageData(const ImageSource& imageSource, vk::Format format, ImageData*& outImageData, std::vector<ImageData*>& allocatedImageData);

private:
    static bool validateFaceImageRegion(const ImageCube* image, ImageCubeFace face, ImageRegion& imageRegion);

    static bool validateEquirectangularImageRegion(const ImageCube* image, ImageRegion& imageRegion);

    static ComputePipeline* getEquirectangularComputePipeline();

    static DescriptorSet* getEquirectangularComputeDescriptorSet();

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

private:
    vk::Image m_image;
    DeviceMemoryBlock* m_memory;
    uint32_t m_size;
    uint32_t m_mipLevelCount;
    vk::Format m_format;

    static ComputePipeline* s_computeEquirectangularPipeline;
    static DescriptorSet* s_computeEquirectangularDescriptorSet;
};


#endif //WORLDENGINE_IMAGECUBE_H
