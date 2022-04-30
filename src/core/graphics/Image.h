
#ifndef WORLDENGINE_IMAGE_H
#define WORLDENGINE_IMAGE_H

#include "core/core.h"

class DeviceMemoryBlock;

enum class ImagePixelLayout {
    Invalid = 0,
    R = 1,
    RG = 2,
    RGB = 3,
    BGR = 4,
    RGBA = 5,
    ABGR = 6,
};

enum class ImagePixelFormat {
    Invalid = 0,
    UInt8 = 1,
    UInt16 = 2,
    UInt32 = 3,
    SInt8 = 4,
    SInt16 = 5,
    SInt32 = 6,
    Float16 = 7,
    Float32 = 8,
};

class ImageData {
    //NO_COPY(ImageData)
private:
    ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, bool stbiAllocated);

public:
    ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

    ~ImageData();

    static ImageData* load(const std::string& filePath, ImagePixelLayout desiredLayout = ImagePixelLayout::Invalid, ImagePixelFormat desiredFormat = ImagePixelFormat::UInt8);

    static void unload(const std::string& filePath);

    static void clearCache();

    static ImageData* mutate(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat);

    uint8_t* getData() const;

    uint32_t getWidth() const;

    uint32_t getHeight() const;

    ImagePixelLayout getPixelLayout() const;

    ImagePixelFormat getPixelFormat() const;

    static int getChannels(ImagePixelLayout layout);

    static int getChannelSize(ImagePixelFormat format);

    static bool getPixelSwizzle(ImagePixelLayout layout, vk::ComponentSwizzle swizzle[4]);

    static bool getPixelLayoutAndFormat(vk::Format format, ImagePixelLayout& outLayout, ImagePixelFormat& outFormat);

private:
    uint8_t* m_data;
    uint32_t m_width;
    uint32_t m_height;
    ImagePixelLayout m_pixelLayout;
    ImagePixelFormat m_pixelFormat;
    bool m_stbiAllocated;
    bool m_externallyAllocated;

    static std::map<std::string, ImageData*> s_imageCache;
};




struct ImageTransitionState {
    vk::ImageLayout layout;
    vk::AccessFlags accessMask = {};
    vk::PipelineStageFlags pipelineStage;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

namespace ImageTransition {
    struct FromAny : public ImageTransitionState { FromAny(); };
    struct TransferSrc : public ImageTransitionState { TransferSrc(); };
    struct TransferDst : public ImageTransitionState { TransferDst(); };
    struct ShaderAccess : public ImageTransitionState { ShaderAccess(vk::PipelineStageFlags shaderPipelineStages, bool read, bool write); };
    struct ShaderReadOnly : public ShaderAccess { ShaderReadOnly(vk::PipelineStageFlags shaderPipelineStages); };
    struct ShaderWriteOnly : public ShaderAccess { ShaderWriteOnly(vk::PipelineStageFlags shaderPipelineStages); };
    struct ShaderReadWrite : public ShaderAccess { ShaderReadWrite(vk::PipelineStageFlags shaderPipelineStages); };
};


struct ImageRegion {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    uint32_t baseLayer = 0;
    uint32_t baseMipLevel = 0;

    uint32_t width = (uint32_t)VK_WHOLE_SIZE;
    uint32_t height = (uint32_t)VK_WHOLE_SIZE;
    uint32_t depth = (uint32_t)VK_WHOLE_SIZE;
    uint32_t layerCount = (uint32_t)VK_WHOLE_SIZE;
    uint32_t mipLevelCount = (uint32_t)VK_WHOLE_SIZE;
};


namespace ImageUtil {
    bool isDepthAttachment(const vk::Format& format);

    bool isStencilAttachment(const vk::Format& format);
}


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
    Image2D(std::weak_ptr<vkr::Device> device, vk::Image image, DeviceMemoryBlock* memory, uint32_t width, uint32_t height, vk::Format format);

public:
    ~Image2D();

    static Image2D* create(const Image2DConfiguration& image2DConfiguration);

    static bool upload(Image2D* dstImage, void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    static bool transitionLayout(Image2D* image, vk::CommandBuffer commandBuffer, vk::ImageAspectFlags aspectMask, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool upload(void* data, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, vk::ImageAspectFlags aspectMask, ImageRegion imageRegion, const ImageTransitionState& dstState);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Image& getImage() const;

    uint32_t getWidth() const;

    uint32_t getHeight() const;

    vk::Format getFormat() const;

    const GraphicsResource& getResourceId() const;

    static vk::Format selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

private:

    static bool validateImageRegion(Image2D* image, ImageRegion& imageRegion);

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
    GraphicsResource m_ResourceId;
};


#endif //WORLDENGINE_IMAGE_H
