#include "core/graphics/BufferView.h"
#include "core/graphics/Buffer.h"


void BufferViewConfiguration::setBuffer(const vk::Buffer& buffer) {
    this->buffer = buffer;
}

void BufferViewConfiguration::setBuffer(const Buffer* buffer) {
    setBuffer(buffer->getBuffer());
}

void BufferViewConfiguration::setFormat(const vk::Format& format) {
    this->format = format;
}

void BufferViewConfiguration::setOffsetRange(const vk::DeviceSize& offset, const vk::DeviceSize& range) {
    this->offset = offset;
    this->range = range;
}

BufferView::BufferView(std::weak_ptr<vkr::Device> device, const vk::BufferView& bufferView, const vk::Format& format, const vk::DeviceSize& offset, const vk::DeviceSize& range):
    m_device(device),
    m_bufferView(bufferView),
    m_format(format),
    m_offset(offset),
    m_range(range) {
}

BufferView::~BufferView() {
    (**m_device).destroyBufferView(m_bufferView);
}

BufferView* BufferView::create(const BufferViewConfiguration& bufferViewConfiguration, const char* name) {
    vk::BufferViewCreateInfo bufferViewCreateInfo;
    bufferViewCreateInfo.buffer = bufferViewConfiguration.buffer;
    bufferViewCreateInfo.format = bufferViewConfiguration.format;
    bufferViewCreateInfo.offset = bufferViewConfiguration.offset;
    bufferViewCreateInfo.range = bufferViewConfiguration.range;

    const vk::Device& device = **bufferViewConfiguration.device.lock();

    vk::BufferView bufferView = VK_NULL_HANDLE;
    vk::Result result = device.createBufferView(&bufferViewCreateInfo, nullptr, &bufferView);
    if (result != vk::Result::eSuccess) {
        printf("Failed to create BufferView: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkBufferView)bufferView, vk::ObjectType::eBufferView, name);

    return new BufferView(bufferViewConfiguration.device, bufferView, bufferViewConfiguration.format, bufferViewConfiguration.offset, bufferViewConfiguration.range);
}

const std::shared_ptr<vkr::Device>& BufferView::getDevice() const {
    return m_device;
}

const vk::BufferView& BufferView::getBufferView() const {
    return m_bufferView;
}

const vk::Format& BufferView::getFormat() const {
    return m_format;
}

const vk::DeviceSize& BufferView::getOffset() const {
    return m_offset;
}

const vk::DeviceSize& BufferView::getRange() const {
    return m_range;
}