
#ifndef WORLDENGINE_BUFFER_H
#define WORLDENGINE_BUFFER_H

#include "core/core.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/FrameResource.h"
#include "core/graphics/GraphicsResource.h"

struct ShutdownGraphicsEvent;

struct BufferConfiguration {
    WeakResource<vkr::Device> device;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::DeviceSize size;
    void* data = nullptr;
};


class Buffer : public GraphicsResource {
    NO_COPY(Buffer)

private:
    Buffer(const WeakResource<vkr::Device>& device, const vk::Buffer& buffer, DeviceMemoryBlock* memory, vk::DeviceSize size, const vk::MemoryPropertyFlags& memoryProperties, const ResourceId& resourceId, const std::string& name);

public:
    //Buffer(Buffer&& buffer);

    ~Buffer() override;

    static Buffer* create(const BufferConfiguration& bufferConfiguration, const std::string& name);

    static bool copy(Buffer* srcBuffer, Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

    static bool upload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data, vk::DeviceSize srcStride = 0, vk::DeviceSize dstStride = 0, vk::DeviceSize elementSize = 0);

    bool copyFrom(Buffer* srcBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

    bool copyTo(Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

    bool upload(vk::DeviceSize offset, vk::DeviceSize size, const void* data, vk::DeviceSize srcStride = 0, vk::DeviceSize dstStride = 0, vk::DeviceSize elementSize = 0);

    const vk::Buffer& getBuffer() const;

    vk::DeviceSize getSize() const;

    template<typename T>
    vk::DeviceSize getSize();

    vk::MemoryPropertyFlags getMemoryProperties() const;

    bool hasMemoryProperties(const vk::MemoryPropertyFlags& memoryProperties, bool any = false);

    void* map();

    template<typename T>
    T* map();

    void unmap();

    bool isMapped() const;

public:
    static const Buffer* getStagingBuffer();

    static bool stagedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data, vk::DeviceSize srcStride = 0, vk::DeviceSize dstStride = 0, vk::DeviceSize elementSize = 0);

    static bool mappedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data, vk::DeviceSize srcStride = 0, vk::DeviceSize dstStride = 0, vk::DeviceSize elementSize = 0);

private:
    static void resizeStagingBuffer(const WeakResource<vkr::Device>& device, vk::DeviceSize size);

    static void reserveStagingBuffer(const WeakResource<vkr::Device>& device, vk::DeviceSize size);

    static void onCleanupGraphics(ShutdownGraphicsEvent* event);

private:
    vk::Buffer m_buffer;
    DeviceMemoryBlock* m_memory;
    vk::MemoryPropertyFlags m_memoryProperties;
    vk::DeviceSize m_size;

    static FrameResource<Buffer> s_stagingBuffer;
    static vk::DeviceSize s_maxStagingBufferSize;
};


template<typename T>
vk::DeviceSize Buffer::getSize() {
    return m_size / sizeof(T);
}

template<typename T>
T* Buffer::map() {
    return static_cast<T*>(map());
}

#endif //WORLDENGINE_BUFFER_H
