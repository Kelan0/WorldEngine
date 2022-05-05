
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

class ImageData {
    //NO_COPY(ImageData)
public:

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
    ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, bool stbiAllocated);

public:
    ImageData(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

    ~ImageData();

    static ImageData* load(const std::string& filePath, ImagePixelLayout desiredLayout = ImagePixelLayout::Invalid, ImagePixelFormat desiredFormat = ImagePixelFormat::UInt8);

    static void unload(const std::string& filePath);

    static void clearCache();

    static ImageData* mutate(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat);

    static ImageData* transform(const ImageData* imageData, const ImageTransform& transformation);

    static ImageData* transform(uint8_t* data, uint32_t width, uint32_t height, ImagePixelLayout layout, ImagePixelFormat format, const ImageTransform& transformation);

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

    static std::unordered_map<std::string, ImageData*> s_imageCache;
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

    bool transitionLayout(const vk::Image& image, vk::CommandBuffer commandBuffer, const vk::ImageSubresourceRange& subresourceRange, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool upload(const vk::Image& dstImage, void* data, const uint32_t& bytesPerPixel, const vk::ImageAspectFlags& aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& dstState);

    bool transferBuffer(const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const vk::ImageAspectFlags& aspectMask, const uint32_t& baseLayer, const uint32_t& layerCount, const uint32_t& baseMipLevel, const uint32_t& mipLevelCount, const ImageTransitionState& dstState);
}


#endif //WORLDENGINE_IMAGEDATA_H
