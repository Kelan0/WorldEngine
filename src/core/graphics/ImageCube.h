
#ifndef WORLDENGINE_IMAGECUBE_H
#define WORLDENGINE_IMAGECUBE_H

#include "core/core.h"
#include "core/graphics/ImageData.h"

class DeviceMemoryBlock;

enum ImageCubeFace {
    ImageCubeFace_PosX = 0,
    ImageCubeFace_NegX = 1,
    ImageCubeFace_PosY = 2,
    ImageCubeFace_NegY = 3,
    ImageCubeFace_PosZ = 4,
    ImageCubeFace_NegZ = 5,
};

struct ImageCubeConfiguration {
    std::weak_ptr<vkr::Device> device;

    std::array<ImageData*, 6> faceImageData = { nullptr };
    std::array<std::string, 6> faceFilePath = { "" };
    std::array<ImageData::ImageTransform*, 6> faceImageTransforms = { nullptr };
    std::string equirectangularFilePath = "";
    uint32_t size = 0;
    uint32_t mipLevels = 1;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    bool enabledTexelAccess = false;
    bool preInitialized = false;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    void setSource(const std::array<ImageData*, 6>& faceImageData, const std::array<ImageData::ImageTransform*, 6>& faceImageTransforms = { nullptr });
    void setSource(const ImageCubeFace& face, ImageData* faceImageData, ImageData::ImageTransform* faceImageTransform = nullptr);
    void setSource(const std::array<std::string, 6>& faceFilePaths, const std::array<ImageData::ImageTransform*, 6>& faceImageTransforms = { nullptr });
    void setSource(const ImageCubeFace& face, const std::string& faceFilePath, ImageData::ImageTransform* faceImageTransform = nullptr);
    void setSource(const std::string& equirectangularFilePath);
};

class ImageCube {
    NO_COPY(ImageCube);
private:
    ImageCube(const std::weak_ptr<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& size, const vk::Format& format);

public:
    ~ImageCube();

    static ImageCube* create(const ImageCubeConfiguration& imageCubeConfiguration);

    static bool upload(ImageCube* dstImage, const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool upload(const ImageCubeFace& face, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Image& getImage() const;

    const uint32_t& getSize() const;

    const uint32_t& getWidth() const;

    const uint32_t& getHeight() const;

    vk::Format getFormat() const;

    const GraphicsResource& getResourceId() const;

    static std::array<bool, 6> loadCubeFacesImageData(const std::array<std::string, 6>& cubeFaceFilePaths, const vk::Format& format, std::array<ImageData*, 6>& outImageData, std::vector<ImageData*>& allocatedImageData);

private:
    static bool validateImageRegion(const ImageCube* image, const ImageCubeFace& face, ImageRegion& imageRegion);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Image m_image;
    DeviceMemoryBlock* m_memory;
    uint32_t m_size;
    vk::Format m_format;
    GraphicsResource m_ResourceId;
};



struct ImageViewCubeConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::Image image;
    vk::Format format;
    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    uint32_t baseMipLevel = 0;
    uint32_t mipLevelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t arrayLayerCount = 1;
    vk::ComponentSwizzle redSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle greenSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle blueSwizzle = vk::ComponentSwizzle::eIdentity;
    vk::ComponentSwizzle alphaSwizzle = vk::ComponentSwizzle::eIdentity;

    void setImage(const vk::Image& image);

    void setImage(const ImageCube* image);
};

class ImageViewCube {
    NO_COPY(ImageViewCube)
private:
    ImageViewCube(std::weak_ptr<vkr::Device> device, vk::ImageView imageView);

public:
    ~ImageViewCube();

    static ImageViewCube* create(const ImageViewCubeConfiguration& imageViewCubeConfiguration);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::ImageView& getImageView() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::ImageView m_imageView;
    GraphicsResource m_resourceId;
};


#endif //WORLDENGINE_IMAGECUBE_H
