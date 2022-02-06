#pragma once

#include "../core.h"
#include "GraphicsManager.h"

struct GPUMemoryConfiguration {
	std::shared_ptr<vkr::Device> device;
	size_t size;
	uint32_t memoryTypeBits;
	vk::MemoryPropertyFlags memoryPropertyFlags;

};

class GPUMemory {
	NO_COPY(GPUMemory);

private:
	GPUMemory(std::shared_ptr<vkr::Device> device, std::unique_ptr<vkr::DeviceMemory> deviceMemory);

public:
	~GPUMemory();

	static GPUMemory* create(const GPUMemoryConfiguration& gpuMemoryConfiguration);

	static bool selectMemoryType(uint32_t memoryTypeBits, vk::MemoryPropertyFlags memoryPropertyFlags, uint32_t& outMemoryType);

	const vk::DeviceMemory& getDeviceMemory() const;

private:
	std::shared_ptr<vkr::Device> m_device;
	std::unique_ptr<vkr::DeviceMemory> m_deviceMemory;
};

