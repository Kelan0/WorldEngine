
#ifndef WORLDENGINE_IMAGEDATA_H
#define WORLDENGINE_IMAGEDATA_H

#include "core/core.h"

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

class ComputePipeline;
class Buffer;

class ImageData {
    //NO_COPY(ImageData)
public:
    enum AllocationType {
        AllocationType_External = 0,
        AllocationType_Internal = 1,
        AllocationType_Stbi = 2,
    };

    struct ImageTransform {
        static std::unordered_map<std::string, ComputePipeline*> s_transformComputePipelines;

        ImageTransform() = default;
        ImageTransform(const ImageTransform& copy) = default;

        virtual ImageData* apply(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format) const;

        virtual bool isNoOp() const;

        static void initComputeResources();

        static void destroyComputeResources();
    };

    typedef ImageTransform NoOp;

    struct Flip : public ImageTransform {
        bool flip_x, flip_y;

        Flip(bool x, bool y);
        Flip(const Flip& copy);

        virtual ImageData* apply(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format) const override;

        virtual bool isNoOp() const override;

        static ComputePipeline* getComputePipeline();
    };

private:
    ImageData(uint8_t* data, const uint32_t& width, const uint32_t& height, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat, const AllocationType& allocationType);

public:
//    ImageData(const uint32_t& width, const uint32_t& height, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat);

    ImageData(uint8_t* data, const uint32_t& width, const uint32_t& height, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat);

    ImageData(const uint32_t& width, const uint32_t& height, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat);

    ~ImageData();

    static ImageData* load(const std::string& filePath, ImagePixelLayout desiredLayout = ImagePixelLayout::Invalid, ImagePixelFormat desiredFormat = ImagePixelFormat::UInt8);

    static void unload(const std::string& filePath);

    static void clearCache();

    static ImageData* mutate(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat);

    static ImageData* transform(const ImageData* imageData, const ImageTransform& transformation);

    static ImageData* transform(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format, const ImageTransform& transformation);

    int64_t getChannel(const size_t& x, const size_t& y, const size_t& channelIndex);
    void setChannel(const size_t& x, const size_t& y, const size_t& channelIndex, const int64_t& value);

    void setPixel(const size_t& x, const size_t& y, const int64_t& r, const int64_t& g, const int64_t& b, const int64_t& a);
    void setPixelf(const size_t& x, const size_t& y, const float& r, const float& g, const float& b, const float& a);

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
    static size_t getChannelOffset(const size_t& x, const size_t& y, const size_t& channelIndex, const uint32_t& width, const uint32_t& height, const ImagePixelLayout& pixelLayout, const ImagePixelFormat& pixelFormat);

private:
    uint8_t* m_data;
    uint32_t m_width;
    uint32_t m_height;
    ImagePixelLayout m_pixelLayout;
    ImagePixelFormat m_pixelFormat;
    AllocationType m_allocationType;

    static std::unordered_map<std::string, ImageData*> s_imageCache;
};




struct ImageTransitionState {
    vk::ImageLayout layout;
    vk::AccessFlags accessMask = {};
    vk::PipelineStageFlags pipelineStage;
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    bool operator==(const ImageTransitionState& other) const;
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
    size_t x = 0;
    size_t y = 0;
    size_t z = 0;
    size_t baseLayer = 0;
    size_t baseMipLevel = 0;

    size_t width = VK_WHOLE_SIZE;
    size_t height = VK_WHOLE_SIZE;
    size_t depth = VK_WHOLE_SIZE;
    size_t layerCount = VK_WHOLE_SIZE;
    size_t mipLevelCount = VK_WHOLE_SIZE;
};


namespace ImageUtil {
    bool isDepthAttachment(const vk::Format& format);

    bool isStencilAttachment(const vk::Format& format);

    vk::Format selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    bool getImageFormatProperties(const vk::Format& format, const vk::ImageType& type, const vk::ImageTiling& tiling, const vk::ImageUsageFlags& usage, const vk::ImageCreateFlags& flags, vk::ImageFormatProperties* imageFormatProperties);

    bool validateImageCreateInfo(const vk::ImageCreateInfo& imageCreateInfo);

    bool transitionLayout(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageSubresourceRange& subresourceRange, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool upload(const vk::Image& dstImage, void* data, const uint32_t& bytesPerPixel, const vk::ImageAspectFlags& aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    bool upload(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, void* data, const uint32_t& bytesPerPixel, const vk::ImageAspectFlags& aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    bool transferBuffer(const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    bool transferBuffer(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    bool generateMipmap(const vk::Image& image, const vk::Format& format, const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& baseLayer, const uint32_t& layerCount, const uint32_t& width, const uint32_t& height, const uint32_t& depth, const uint32_t& mipLevels, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    bool generateMipmap(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::Format& format, const vk::Filter& filter, const vk::ImageAspectFlags& aspectMask, const uint32_t& baseLayer, const uint32_t& layerCount, const uint32_t& width, const uint32_t& height, const uint32_t& depth, const uint32_t& mipLevels, const ImageTransitionState& dstState, const ImageTransitionState& srcState = ImageTransition::FromAny());

    uint32_t getMaxMipLevels(const uint32_t& width, const uint32_t& height, const uint32_t& depth);

    bool checkAllImageFormatFeatures(const vk::Format& format, const vk::ImageTiling& tiling, const vk::FormatFeatureFlags& formatFeatureFlags);

    bool checkAnyImageFormatFeatures(const vk::Format& format, const vk::ImageTiling& tiling, const vk::FormatFeatureFlags& formatFeatureFlags);

    const vk::CommandBuffer& beginTransferCommands();

    void endTransferCommands(const vk::Queue& queue, const bool& waitComplete);

    const vk::CommandBuffer& getTransferCommandBuffer();

    const vk::CommandBuffer& getComputeCommandBuffer();

    const vk::Queue& getComputeQueue();

    vk::DeviceSize getImageSizeBytes(const ImageRegion& imageRegion, const uint32_t& bytesPerPixel);

    vk::DeviceSize getImageSizeBytes(const uint32_t& width, const uint32_t& height, const uint32_t& depth, const uint32_t& bytesPerPixel);

    Buffer* getImageStagingBuffer(const ImageRegion& imageRegion, const uint32_t& bytesPerPixel);

    Buffer* getImageStagingBuffer(const uint32_t& width, const uint32_t& height, const uint32_t& depth, const uint32_t& bytesPerPixel);
}


#endif //WORLDENGINE_IMAGEDATA_H
