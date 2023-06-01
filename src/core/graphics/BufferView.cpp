#include "core/graphics/BufferView.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/GraphicsManager.h"
#include "core/util/Logger.h"


void BufferViewConfiguration::setBuffer(const vk::Buffer& buffer) {
    this->buffer = buffer;
}

void BufferViewConfiguration::setBuffer(const Buffer* buffer) {
    setBuffer(buffer->getBuffer());
}

void BufferViewConfiguration::setFormat(vk::Format format) {
    this->format = format;
}

void BufferViewConfiguration::setOffsetRange(vk::DeviceSize offset, vk::DeviceSize range) {
    this->offset = offset;
    this->range = range;
}

BufferView::BufferView(const WeakResource<vkr::Device>& device, const vk::BufferView& bufferView, vk::Format format, vk::DeviceSize offset, vk::DeviceSize range, const std::string& name):
        GraphicsResource(ResourceType_BufferView, device, name),
        m_bufferView(bufferView),
        m_format(format),
        m_offset(offset),
        m_range(range) {
}

BufferView::~BufferView() {
    (**m_device).destroyBufferView(m_bufferView);
}

BufferView* BufferView::create(const BufferViewConfiguration& bufferViewConfiguration, const std::string& name) {
    vk::BufferViewCreateInfo bufferViewCreateInfo;
    bufferViewCreateInfo.buffer = bufferViewConfiguration.buffer;
    bufferViewCreateInfo.format = bufferViewConfiguration.format;
    bufferViewCreateInfo.offset = bufferViewConfiguration.offset;
    bufferViewCreateInfo.range = bufferViewConfiguration.range;

    const vk::Device& device = **bufferViewConfiguration.device.lock(name);

    vk::BufferView bufferView = nullptr;
    vk::Result result = device.createBufferView(&bufferViewCreateInfo, nullptr, &bufferView);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create BufferView: %s", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkBufferView)bufferView, vk::ObjectType::eBufferView, name);

    return new BufferView(bufferViewConfiguration.device, bufferView, bufferViewConfiguration.format, bufferViewConfiguration.offset, bufferViewConfiguration.range, name);
}

const vk::BufferView& BufferView::getBufferView() const {
    return m_bufferView;
}

vk::Format BufferView::getFormat() const {
    return m_format;
}

vk::DeviceSize BufferView::getOffset() const {
    return m_offset;
}

vk::DeviceSize BufferView::getRange() const {
    return m_range;
}