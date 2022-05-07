
#ifndef WORLDENGINE_IMAGECUBE_H
#define WORLDENGINE_IMAGECUBE_H

#include "core/core.h"
#include "core/graphics/ImageData.h"
#include "core/engine/scene/event/Events.h"

class DeviceMemoryBlock;
class ComputePipeline;
class DescriptorSet;

enum ImageCubeFace {
    ImageCubeFace_PosX = 0,
    ImageCubeFace_NegX = 1,
    ImageCubeFace_PosY = 2,
    ImageCubeFace_NegY = 3,
    ImageCubeFace_PosZ = 4,
    ImageCubeFace_NegZ = 5,
};

struct ImageSource {
    ImageData* imageData = nullptr;
    std::string filePath = "";
    ImageData::ImageTransform* imageTransform = nullptr;

    void setImageData(ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);
    void setFilePath(const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);
    void setImageTransform(ImageData::ImageTransform* imageTransform);

    bool hasSource() const;
};

struct ImageCubeSource {
    std::array<ImageSource, 6> faceImages = {};
    ImageSource equirectangularImage = {};

    void setFaceSource(const ImageCubeFace& face, const ImageSource& imageSource);
    void setFaceSource(const ImageCubeFace& face, ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);
    void setFaceSource(const ImageCubeFace& face, const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);
    void setEquirectangularSource(const ImageSource& imageSource);
    void setEquirectangularSource(ImageData* imageData, ImageData::ImageTransform* imageTransform = nullptr);
    void setEquirectangularSource(const std::string& filePath, ImageData::ImageTransform* imageTransform = nullptr);

    bool isEquirectangular() const;
};

struct ImageCubeConfiguration {
    std::weak_ptr<vkr::Device> device;

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

class ImageCube {
    NO_COPY(ImageCube);
private:
    ImageCube(const std::weak_ptr<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& size, const uint32_t& mipLevelCount, const vk::Format& format);

public:
    ~ImageCube();

    static ImageCube* create(const ImageCubeConfiguration& imageCubeConfiguration);

    static bool uploadFace(ImageCube* dstImage, const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool uploadEquirectangular(ImageCube* dstImage, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool uploadFace(const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool uploadEquirectangular(void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool generateMipmap(ImageCube* image, const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState);

    bool generateMipmap(const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& mipLevels, const ImageTransitionState& dstState);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Image& getImage() const;

    const uint32_t& getSize() const;

    const uint32_t& getWidth() const;

    const uint32_t& getHeight() const;

    const uint32_t getMipLevelCount() const;

    vk::Format getFormat() const;

    const GraphicsResource& getResourceId() const;

    static std::array<bool, 6> loadCubeFacesImageData(const std::array<ImageSource, 6>& cubeFaceImageSources, const vk::Format& format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData);

    static bool loadImageData(const ImageSource& imageSource, const vk::Format& format, ImageData*& outImageData, std::vector<ImageData*>& allocatedImageData);

private:
    static bool validateFaceImageRegion(const ImageCube* image, const ImageCubeFace& face, ImageRegion& imageRegion);

    static bool validateEquirectangularImageRegion(const ImageCube* image, ImageRegion& imageRegion);

    static ComputePipeline* getEquirectangularComputePipeline();
    static DescriptorSet* getEquirectangularComputeDescriptorSet();
    static void onCleanupGraphics(const ShutdownGraphicsEvent& event);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Image m_image;
    DeviceMemoryBlock* m_memory;
    uint32_t m_size;
    uint32_t m_mipLevelCount;
    vk::Format m_format;
    GraphicsResource m_ResourceId;

    static ComputePipeline* s_computeEquirectangularPipeline;
    static DescriptorSet* s_computeEquirectangularDescriptorSet;
};


#endif //WORLDENGINE_IMAGECUBE_H
