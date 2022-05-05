
#ifndef WORLDENGINE_BUFFERVIEW_H
#define WORLDENGINE_BUFFERVIEW_H

#include "core/core.h"

class Buffer;

struct BufferViewConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::Buffer buffer = VK_NULL_HANDLE;
    vk::Format format = vk::Format::eUndefined;
    vk::DeviceSize offset = 0;
    vk::DeviceSize range = 0;

    void setBuffer(const vk::Buffer& buffer);

    void setBuffer(const Buffer* buffer);

    void setFormat(const vk::Format& format);

    void setOffsetRange(const vk::DeviceSize& offset, const vk::DeviceSize& range);
};

class BufferView {
    NO_COPY(BufferView)
private:
    BufferView(std::weak_ptr<vkr::Device> device, const vk::BufferView& bufferView, const vk::Format& format, const vk::DeviceSize& offset, const vk::DeviceSize& range);

public:
    ~BufferView();

    static BufferView* create(const BufferViewConfiguration& bufferViewConfiguration);

    const std::shared_ptr<vkr::Device>& getDevice() const;

    const vk::BufferView& getBufferView() const;

    const vk::Format& getFormat() const;

    const vk::DeviceSize& getOffset() const;

    const vk::DeviceSize& getRange() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::BufferView m_bufferView;
    vk::Format m_format;
    vk::DeviceSize m_offset;
    vk::DeviceSize m_range;
};


#endif //WORLDENGINE_BUFFERVIEW_H
