#pragma once

#include "../core.h"
#include "GraphicsManager.h"

struct BufferConfiguration {
	std::shared_ptr<vkr::Device> device;
	vk::BufferUsageFlags usage;
	vk::MemoryPropertyFlags memoryProperties;
	vk::DeviceSize size;
	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	void* data = NULL;
};


class Buffer {
	NO_COPY(Buffer);
private:
	Buffer(std::shared_ptr<vkr::Device> device, std::unique_ptr<vkr::Buffer> buffer, std::shared_ptr<vkr::DeviceMemory> deviceMemory, vk::DeviceSize size, vk::MemoryPropertyFlags memoryProperties);

public:
	~Buffer();

	static Buffer* create(const BufferConfiguration& bufferConfiguration);

	static bool copy(Buffer* srcBuffer, Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset = 0, vk::DeviceSize dstOffset = 0);

	const vk::Buffer& getBuffer() const;

	vk::DeviceSize getSize() const;

	bool upload(vk::DeviceSize offset, vk::DeviceSize size, void* data);

	template<typename T>
	bool upload(size_t index, size_t count, T* data);

	template<typename T>
	bool upload(size_t index, const std::vector<T>& data);

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unique_ptr<vkr::Buffer> m_buffer;
	std::shared_ptr<vkr::DeviceMemory> m_deviceMemory;
	vk::MemoryPropertyFlags m_memoryProperties;
	vk::DeviceSize m_size;
};

template<typename T>
inline bool Buffer::upload(size_t index, size_t count, T* data) {
	return upload(sizeof(T) * index, sizeof(T) * count, static_cast<void*>(data));
}

template<typename T>
inline bool Buffer::upload(size_t index, const std::vector<T>& data) {
	return upload(sizeof(T) * index, sizeof(T) * data.size(), static_cast<void*>(data.data()));
}
