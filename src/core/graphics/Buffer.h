#pragma once

#include "../core.h"
#include "GraphicsManager.h"

struct BufferConfiguration {
	std::shared_ptr<vkr::Device> device;
	vk::BufferUsageFlags usage;
	vk::MemoryPropertyFlags memoryProperties;
	size_t size;
	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	void* data = NULL;
};


class Buffer {
	NO_COPY(Buffer);
private:
	Buffer(std::shared_ptr<vkr::Device> device, std::unique_ptr<vkr::Buffer> buffer, std::shared_ptr<vkr::DeviceMemory> deviceMemory, size_t size, vk::MemoryPropertyFlags memoryProperties);

public:
	~Buffer();

	static Buffer* create(const BufferConfiguration& bufferConfiguration);

	static bool copy(Buffer* srcBuffer, Buffer* dstBuffer, size_t size, size_t srcOffset = 0, size_t dstOffset = 0);

	const vk::Buffer& getBuffer() const;

	size_t getSize() const;

	bool upload(size_t offset, size_t size, void* data);

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unique_ptr<vkr::Buffer> m_buffer;
	std::shared_ptr<vkr::DeviceMemory> m_deviceMemory;
	vk::MemoryPropertyFlags m_memoryProperties;
	size_t m_size;
};

