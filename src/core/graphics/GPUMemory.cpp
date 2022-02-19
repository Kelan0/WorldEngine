#include "GPUMemory.h"
#include "../application/Application.h"

GPUMemory::GPUMemory(std::shared_ptr<vkr::Device> device, std::unique_ptr<vkr::DeviceMemory> deviceMemory):
	m_device(std::move(device)),
	m_deviceMemory(std::move(deviceMemory)) {
}

GPUMemory::~GPUMemory() {
}

GPUMemory* GPUMemory::create(const GPUMemoryConfiguration& gpuMemoryConfiguration) {
	float printAllocSize = (float)gpuMemoryConfiguration.size;
	std::string printSizeUnit = "Bytes";

	if (printAllocSize >= 1024) {
		printAllocSize /= 1024;
		printSizeUnit = "KiB";
		if (printAllocSize >= 1024) {
			printAllocSize /= 1024;
			printSizeUnit = "MiB";
			if (printAllocSize >= 1024) {
				printAllocSize /= 1024;
				printSizeUnit = "GiB";
			}
		}
	}

	printf("Allocating %g %s GPU memory\n", printAllocSize, printSizeUnit.c_str());

	uint32_t memoryTypeIndex;
	if (!selectMemoryType(gpuMemoryConfiguration.memoryTypeBits, gpuMemoryConfiguration.memoryPropertyFlags, memoryTypeIndex)) {
		printf("Unable to allocate GPU memory. Required memory type and properties were not found for the device\n");
		return NULL;
	}

	std::unique_ptr<vkr::DeviceMemory> memory = NULL;

	try {
		vk::MemoryAllocateInfo allocateInfo;
		allocateInfo.setAllocationSize(gpuMemoryConfiguration.size);
		allocateInfo.setMemoryTypeIndex(memoryTypeIndex);
		memory = std::make_unique<vkr::DeviceMemory>(*gpuMemoryConfiguration.device, allocateInfo);
	} catch (std::exception e) {
		printf("Failed to allocate GPU memory: %s\n", e.what());
	}

	if (memory == NULL) {
		return NULL;
	}

	return new GPUMemory(gpuMemoryConfiguration.device, std::move(memory));
}

bool GPUMemory::selectMemoryType(uint32_t memoryTypeBits, vk::MemoryPropertyFlags memoryPropertyFlags, uint32_t& outMemoryType) {

	const vk::PhysicalDeviceMemoryProperties& deviceMemProps = Application::instance()->graphics()->getDeviceMemoryProperties();

	for (uint32_t i = 0; i < deviceMemProps.memoryTypeCount; ++i) {
		if ((memoryTypeBits & (1 << i)) != 0 && (deviceMemProps.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
			outMemoryType = i;
			return true;
		}
	}

	return false;
}

const vk::DeviceMemory& GPUMemory::getDeviceMemory() const {
	return **m_deviceMemory;
}
