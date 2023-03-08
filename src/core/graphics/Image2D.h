
#ifndef WORLDENGINE_IMAGE2D_H
#define WORLDENGINE_IMAGE2D_H

#include "core/core.h"
#include "core/graphics/ImageData.h"
#include "core/graphics/GraphicsResource.h"

class DeviceMemoryBlock;


struct Image2DConfiguration {
    WeakResource<vkr::Device> device;
    const ImageData* imageData = nullptr;
    std::string filePath;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    bool mutableFormat = false;
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
    bool enabledTexelAccess = false; // Linear tiling
    bool preInitialized = false;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    bool generateMipmap = false;

    void setSize(uint32_t width, uint32_t height);
    void setSize(const glm::uvec2& size);
    void setSize(const vk::Extent2D& size);

    void setSource(const ImageData* imageData);
    void setSource(const std::string& filePath);
};

class Image2D : public GraphicsResource {
    NO_COPY(Image2D);
private:
    Image2D(const WeakResource<vkr::Device>& device, const vk::Image& image, DeviceMemoryBlock* memory, uint32_t width, uint32_t height, uint32_t mipLevelCount, vk::Format format, const std::string& name);

public:
    ~Image2D() override;

    static Image2D* create(const Image2DConfiguration& image2DConfiguration, const std::string& name);

    static bool upload(Image2D* dstImage, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    bool upload(void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool generateMipmap(Image2D* image, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& dstState);

    bool generateMipmap(vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t mipLevels, const ImageTransitionState& dstState);

    const vk::Image& getImage() const;

    uint32_t getWidth() const;

    uint32_t getHeight() const;

    glm::uvec2 getResolution() const;

    uint32_t getMipLevelCount() const;

    vk::Format getFormat() const;

private:
    static bool validateImageRegion(const Image2D* image, ImageRegion& imageRegion);

private:
    vk::Image m_image;
    DeviceMemoryBlock* m_memory;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipLevelCount;
    vk::Format m_format;
};


#endif //WORLDENGINE_IMAGE2D_H
