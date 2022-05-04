
#ifndef WORLDENGINE_IMAGE2D_H
#define WORLDENGINE_IMAGE2D_H

#include "core/core.h"
#include "core/graphics/ImageData.h"

class DeviceMemoryBlock;


struct Image2DConfiguration {
    std::weak_ptr<vkr::Device> device;
    const ImageData* imageData = NULL;
    std::string filePath = "";
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    bool enabledTexelAccess = false;
    bool preInitialized = false;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;

    void setSize(const uint32_t& width, const uint32_t& height);
    void setSize(const glm::uvec2& size);
    void setSize(const vk::Extent2D& size);

    void setSource(const ImageData* imageData);
    void setSource(const std::string& filePath);
};

class Image2D {
    NO_COPY(Image2D);
private:
    Image2D(const std::weak_ptr<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, const uint32_t& width, const uint32_t& height, const vk::Format& format);

public:
    ~Image2D();

    static Image2D* create(const Image2DConfiguration& image2DConfiguration);

    static bool upload(Image2D* dstImage, void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool upload(void* data, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const vk::ImageAspectFlags& aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Image& getImage() const;

    const uint32_t& getWidth() const;

    const uint32_t& getHeight() const;

    const vk::Format& getFormat() const;

    const GraphicsResource& getResourceId() const;

private:
    static bool validateImageRegion(const Image2D* image, ImageRegion& imageRegion);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Image m_image;
    DeviceMemoryBlock* m_memory;
    uint32_t m_width;
    uint32_t m_height;
    vk::Format m_format;
    GraphicsResource m_ResourceId;
};



struct ImageView2DConfiguration {
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

    void setImage(const Image2D* image);
};

class ImageView2D {
    NO_COPY(ImageView2D)
private:
    ImageView2D(std::weak_ptr<vkr::Device> device, vk::ImageView imageView);

public:
    ~ImageView2D();

    static ImageView2D* create(const ImageView2DConfiguration& imageView2DConfiguration);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::ImageView& getImageView() const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::ImageView m_imageView;
    GraphicsResource m_resourceId;
};


#endif //WORLDENGINE_IMAGE2D_H
