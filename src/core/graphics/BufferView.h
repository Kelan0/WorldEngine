
#ifndef WORLDENGINE_BUFFERVIEW_H
#define WORLDENGINE_BUFFERVIEW_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

class Buffer;

struct BufferViewConfiguration {
    WeakResource<vkr::Device> device;
    vk::Buffer buffer = VK_NULL_HANDLE;
    vk::Format format = vk::Format::eUndefined;
    vk::DeviceSize offset = 0;
    vk::DeviceSize range = 0;

    void setBuffer(const vk::Buffer& buffer);

    void setBuffer(const Buffer* buffer);

    void setFormat(vk::Format format);

    void setOffsetRange(vk::DeviceSize offset, vk::DeviceSize range);
};

class BufferView : public GraphicsResource {
    NO_COPY(BufferView)

private:
    BufferView(const WeakResource<vkr::Device>& device, const vk::BufferView& bufferView, vk::Format format, vk::DeviceSize offset, vk::DeviceSize range, const std::string& name);

public:
    ~BufferView() override;

    static BufferView* create(const BufferViewConfiguration& bufferViewConfiguration, const std::string& name);

    const vk::BufferView& getBufferView() const;

    vk::Format getFormat() const;

    vk::DeviceSize getOffset() const;

    vk::DeviceSize getRange() const;

private:
    vk::BufferView m_bufferView;
    vk::Format m_format;
    vk::DeviceSize m_offset;
    vk::DeviceSize m_range;
};


#endif //WORLDENGINE_BUFFERVIEW_H
