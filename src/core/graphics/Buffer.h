#pragma once

#include "../core.h"
#include "GraphicsManager.h"

struct BufferConfiguration {
	std::weak_ptr<vkr::Device> device;
	vk::BufferUsageFlags usage;
	vk::MemoryPropertyFlags memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
	vk::DeviceSize size;
	void* data = NULL;
};


class Buffer {
	NO_COPY(Buffer)
private:
	Buffer(std::weak_ptr<vkr::Device> device, vk::Buffer buffer, vk::DeviceMemory deviceMemory, vk::DeviceSize size, vk::MemoryPropertyFlags memoryProperties);

public:
	//Buffer(Buffer&& buffer);

	~Buffer();

	static Buffer* create(const BufferConfiguration& bufferConfiguration);

	static bool copy(Buffer* srcBuffer, Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

	static bool upload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data);

	bool copyFrom(Buffer* srcBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

	bool copyTo(Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

	bool upload(vk::DeviceSize offset, vk::DeviceSize size, const void* data);

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::Buffer& getBuffer() const;

	vk::DeviceSize getSize() const;

	vk::MemoryPropertyFlags getMemoryProperties() const;

	bool hasMemoryProperties(vk::MemoryPropertyFlags memoryProperties, bool any = false);

public:
	static const std::unique_ptr<Buffer>& getStagingBuffer();

	static void resetStagingBuffer();

	static bool stagedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data);

	static bool mappedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data);

private:
	static void resizeStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size);

	static void reserveStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size);

private:
	std::shared_ptr<vkr::Device> m_device;
	vk::Buffer m_buffer;
	vk::DeviceMemory m_deviceMemory;
	vk::MemoryPropertyFlags m_memoryProperties;
	vk::DeviceSize m_size;

	static std::unique_ptr<Buffer> s_stagingBuffer;
	static vk::DeviceSize s_maxStagingBufferSize;
};