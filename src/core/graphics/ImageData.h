
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
    Count = 7,
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
    Count = 9,
};

struct ImageRegion {
    typedef uint32_t size_type;
    typedef uint32_t offset_type;
    static constexpr size_type WHOLE_SIZE = (size_type)(-1);

    offset_type x = 0;
    offset_type y = 0;
    offset_type z = 0;
    size_type baseLayer = 0;
    size_type baseMipLevel = 0;

    size_type width = WHOLE_SIZE;
    size_type height = WHOLE_SIZE;
    size_type depth = WHOLE_SIZE;
    size_type layerCount = WHOLE_SIZE;
    size_type mipLevelCount = WHOLE_SIZE;

    ImageRegion& setOffset(offset_type x, offset_type y, offset_type z);
    ImageRegion& setOffset(const glm::uvec3& offset);

    ImageRegion& setSize(size_type width, size_type height, size_type depth);
    ImageRegion& setSize(const glm::uvec3& size);
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

        virtual ImageData* apply(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format) const;

        virtual bool isNoOp() const;

        static void initComputeResources();

        static void destroyComputeResources();
    };

    typedef ImageTransform NoOp;

    struct Flip : public ImageTransform {
        bool flip_x, flip_y;

        Flip(bool x, bool y);
        Flip(const Flip& copy);

        virtual ImageData* apply(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format) const override;

        virtual bool isNoOp() const override;

        static ComputePipeline* getComputePipeline();
    };

private:
    ImageData(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat, AllocationType allocationType);

public:
//    ImageData(ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

    ImageData(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

    ImageData(ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

    ~ImageData();

    static ImageData* load(const std::string& filePath, ImagePixelLayout desiredLayout = ImagePixelLayout::Invalid, ImagePixelFormat desiredFormat = ImagePixelFormat::UInt8);

    static void unload(const std::string& filePath);

    static void clearCache();

    static ImageData* mutate(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout srcLayout, ImagePixelFormat srcFormat, ImagePixelLayout dstLayout, ImagePixelFormat dstFormat);

    static ImageData* transform(const ImageData* imageData, const ImageTransform& transformation);

    static ImageData* transform(void* data, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout layout, ImagePixelFormat format, const ImageTransform& transformation);

    float getChannelf(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex) const;

    int64_t getChannel(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex) const;
    void setChannel(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex, int64_t value);

    void setPixel(ImageRegion::offset_type x, ImageRegion::offset_type y, int64_t r, int64_t g, int64_t b, int64_t a);
    void setPixelf(ImageRegion::offset_type x, ImageRegion::offset_type y, float r, float g, float b, float a);

    void* getData() const;

    ImageRegion::size_type getWidth() const;

    ImageRegion::size_type getHeight() const;

    ImagePixelLayout getPixelLayout() const;

    ImagePixelFormat getPixelFormat() const;

    static int getChannels(ImagePixelLayout layout);

    static int getChannelSize(ImagePixelFormat format);

    static bool getPixelSwizzle(ImagePixelLayout layout, vk::ComponentSwizzle swizzle[4]);

    static bool getPixelLayoutAndFormat(vk::Format format, ImagePixelLayout& outLayout, ImagePixelFormat& outFormat);

private:
    static size_t getChannelOffset(ImageRegion::offset_type x, ImageRegion::offset_type y, size_t channelIndex, ImageRegion::size_type width, ImageRegion::size_type height, ImagePixelLayout pixelLayout, ImagePixelFormat pixelFormat);

private:
    void* m_data;
    ImageRegion::size_type m_width;
    ImageRegion::size_type m_height;
    ImagePixelLayout m_pixelLayout;
    ImagePixelFormat m_pixelFormat;
    AllocationType m_allocationType;

    static std::unordered_map<std::string, ImageData*> s_imageCache;
};




struct ImageTransitionState {
    vk::ImageLayout layout; // The layout of the image for this synchronization scope
    vk::AccessFlags accessMask = {}; // The memory access types that this synchronization scope covers.
    vk::PipelineStageFlags pipelineStages; // The pipeline stages that this synchronization scope covers
    uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Queue family ownership transfer

    ImageTransitionState(vk::ImageLayout layout, vk::AccessFlags accessMask, vk::PipelineStageFlags pipelineStages, uint32_t queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

    bool operator==(const ImageTransitionState& other) const;
};

namespace ImageTransition {
    struct FromAny : public ImageTransitionState { FromAny(vk::PipelineStageFlagBits shaderPipelineStages = vk::PipelineStageFlagBits::eTopOfPipe); }; // Shouldn't this be eBottomOfPipe ? Will changing it break something? This is a problem for future me....
    struct General : public ImageTransitionState { General(vk::PipelineStageFlagBits shaderPipelineStages = vk::PipelineStageFlagBits::eTopOfPipe); };
    struct TransferSrc : public ImageTransitionState { TransferSrc(); };
    struct TransferDst : public ImageTransitionState { TransferDst(); };
    struct ShaderAccess : public ImageTransitionState { ShaderAccess(vk::PipelineStageFlags shaderPipelineStages, bool read, bool write); };
    struct ShaderReadOnly : public ShaderAccess { ShaderReadOnly(vk::PipelineStageFlags shaderPipelineStages); };
    struct ShaderWriteOnly : public ShaderAccess { ShaderWriteOnly(vk::PipelineStageFlags shaderPipelineStages); };
    struct ShaderReadWrite : public ShaderAccess { ShaderReadWrite(vk::PipelineStageFlags shaderPipelineStages); };
    struct PresentSrc  : public ImageTransitionState { PresentSrc(); };
};


namespace ImageUtil {
    bool isDepthAttachment(vk::Format format);

    bool isStencilAttachment(vk::Format format);

    bool isColourAttachment(vk::Format format);

    vk::Format selectSupportedFormat(const vk::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    bool getImageFormatProperties(vk::Format format, vk::ImageType type, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageCreateFlags flags, vk::ImageFormatProperties* imageFormatProperties);

    bool validateImageCreateInfo(const vk::ImageCreateInfo& imageCreateInfo);

    bool transitionLayout(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageSubresourceRange& subresourceRange, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool upload(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, void* data, uint32_t bytesPerPixel, vk::ImageAspectFlags aspectMask, const ImageRegion& imageRegion, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool transferBufferToImage(const vk::CommandBuffer& commandBuffer, const vk::Image& dstImage, const vk::Buffer& srcBuffer, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& srcState, const ImageTransitionState& dstState);

    bool transferImageToBuffer(const vk::CommandBuffer& commandBuffer, const vk::Buffer& dstBuffer, const vk::Image& srcImage, const vk::BufferImageCopy& imageCopy, const ImageTransitionState& srcState, const ImageTransitionState& dstState, int a);

    bool generateMipmap(const vk::CommandBuffer& commandBuffer, const vk::Image& image, vk::Format format, vk::Filter filter, vk::ImageAspectFlags aspectMask, uint32_t baseLayer, uint32_t layerCount, ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t mipLevels, const ImageTransitionState& srcState, const ImageTransitionState& dstState, int a);

    uint32_t getMaxMipLevels(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth);

    bool checkAllImageFormatFeatures(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags formatFeatureFlags);

    bool checkAnyImageFormatFeatures(vk::Format format, vk::ImageTiling tiling, vk::FormatFeatureFlags formatFeatureFlags);

    const vk::CommandBuffer& beginTransferCommands();

    void endTransferCommands(const vk::CommandBuffer& transferCommandBuffer, const vk::Queue& queue, bool waitComplete, const vk::Fence& fence);

    const vk::CommandBuffer& getTransferCommandBuffer();

    const vk::CommandBuffer& getComputeCommandBuffer();

    const vk::Queue& getComputeQueue();

    vk::DeviceSize getImageSizeBytes(const ImageRegion& imageRegion, uint32_t bytesPerPixel);

    vk::DeviceSize getImageSizeBytes(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, uint32_t bytesPerPixel);

    vk::DeviceSize getImageSizeBytes(ImageRegion::size_type width, ImageRegion::size_type height, ImageRegion::size_type depth, ImageRegion::size_type layers, uint32_t bytesPerPixel);

    Buffer* getImageStagingBuffer(const ImageRegion& imageRegion, uint32_t bytesPerPixel);
}


#endif //WORLDENGINE_IMAGEDATA_H
