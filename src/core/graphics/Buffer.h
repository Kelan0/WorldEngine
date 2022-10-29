
#ifndef WORLDENGINE_BUFFER_H
#define WORLDENGINE_BUFFER_H

#include "core/core.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/FrameResource.h"

struct ShutdownGraphicsEvent;

struct BufferConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::DeviceSize size;
    void* data = nullptr;
};


class Buffer {
    NO_COPY(Buffer)
private:
    Buffer(const std::weak_ptr<vkr::Device>& device, const vk::Buffer& buffer, DeviceMemoryBlock* memory, const vk::DeviceSize& size, const vk::MemoryPropertyFlags& memoryProperties, const GraphicsResource& resourceId);

public:
    //Buffer(Buffer&& buffer);

    ~Buffer();

    static Buffer* create(const BufferConfiguration& bufferConfiguration, const char* name);

    static bool copy(Buffer* srcBuffer, Buffer* dstBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset = 0, const vk::DeviceSize& dstOffset = 0);

    static bool upload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data);

    bool copyFrom(Buffer* srcBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset = 0, const vk::DeviceSize& dstOffset = 0);

    bool copyTo(Buffer* dstBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset = 0, const vk::DeviceSize& dstOffset = 0);

    bool upload(const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::Buffer& getBuffer() const;

    vk::DeviceSize getSize() const;

    vk::MemoryPropertyFlags getMemoryProperties() const;

    bool hasMemoryProperties(const vk::MemoryPropertyFlags& memoryProperties, const bool& any = false);

    void* map();

    void unmap();

    bool isMapped() const;

    const GraphicsResource& getResourceId() const;

public:
    static const Buffer* getStagingBuffer();

    static bool stagedUpload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data);

    static bool mappedUpload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data);

private:
    static void resizeStagingBuffer(const std::weak_ptr<vkr::Device>& device, const vk::DeviceSize& size);

    static void reserveStagingBuffer(const std::weak_ptr<vkr::Device>& device, const vk::DeviceSize& size);

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::Buffer m_buffer;
    DeviceMemoryBlock* m_memory;
    vk::MemoryPropertyFlags m_memoryProperties;
    vk::DeviceSize m_size;

    GraphicsResource m_resourceId;

    static FrameResource<Buffer> s_stagingBuffer;
    static vk::DeviceSize s_maxStagingBufferSize;
};

#endif //WORLDENGINE_BUFFER_H
