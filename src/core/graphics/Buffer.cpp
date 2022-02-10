#include "Buffer.h"
#include "GPUMemory.h"
#include "CommandPool.h"
#include "../Application.h"

Buffer::Buffer(std::shared_ptr<vkr::Device> device, vk::Buffer buffer, vk::DeviceMemory deviceMemory, vk::DeviceSize size, vk::MemoryPropertyFlags memoryProperties):
	m_device(std::move(device)),
	m_buffer(buffer),
	m_deviceMemory(deviceMemory),
	m_size(size),
	m_memoryProperties(memoryProperties) {
}

//Buffer::Buffer(Buffer&& buffer) {
//	m_device = vkr::exchange(buffer.m_device, {});
//	m_buffer = vkr::exchange(buffer.m_buffer, {});
//	m_deviceMemory = vkr::exchange(buffer.m_deviceMemory, {});
//	m_size = vkr::exchange(buffer.m_size, 0);
//	m_memoryProperties = vkr::exchange(buffer.m_memoryProperties, {});
//}

Buffer::~Buffer() {
	(**m_device).destroyBuffer(m_buffer);
	(**m_device).freeMemory(m_deviceMemory);
}

Buffer* Buffer::create(const BufferConfiguration& bufferConfiguration) {
	vk::BufferUsageFlags usage = bufferConfiguration.usage;

	const vk::Device& device = **bufferConfiguration.device;

	if (bufferConfiguration.data != NULL && !(bufferConfiguration.memoryProperties & vk::MemoryPropertyFlagBits::eHostVisible)) {
		// We have data to upload but the buffer is not visible to the host, so we need to allow this 
		// buffer to be a transfer destination, so the data can be copied from a temporary buffer
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	vk::BufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.setUsage(usage);
	bufferCreateInfo.setSize(bufferConfiguration.size);
	bufferCreateInfo.setSharingMode(bufferConfiguration.sharingMode);
	vk::Buffer buffer = VK_NULL_HANDLE;

	try {
		buffer = device.createBuffer(bufferCreateInfo);
	} catch (std::exception e) {
		printf("Failed to create buffer: %s\n", e.what());
	}

	if (!buffer) {
		return NULL;
	}

	;
	const vk::MemoryRequirements& memoryRequirements = device.getBufferMemoryRequirements(buffer);

	uint32_t memoryTypeIndex;
	if (!GPUMemory::selectMemoryType(memoryRequirements.memoryTypeBits, bufferConfiguration.memoryProperties, memoryTypeIndex)) {
		printf("Vertex buffer memory allocation failed\n");
		return NULL;
	}

	vk::MemoryAllocateInfo allocateInfo;
	allocateInfo.setAllocationSize(memoryRequirements.size);
	allocateInfo.setMemoryTypeIndex(memoryTypeIndex);

	vk::DeviceMemory deviceMemory = VK_NULL_HANDLE;

	try {
		deviceMemory = device.allocateMemory(allocateInfo);
	} catch (std::exception e) {
		printf("Failed to allocate device memory for buffer: %s\n", e.what());
	}

	if (!deviceMemory) {
		return NULL;
	}

	device.bindBufferMemory(buffer, deviceMemory, 0);

	Buffer* returnBuffer = new Buffer(bufferConfiguration.device, buffer, deviceMemory, bufferConfiguration.size, bufferConfiguration.memoryProperties);
	
	if (bufferConfiguration.data != NULL) {
		if (!returnBuffer->upload(0, bufferConfiguration.size, bufferConfiguration.data)) {
			printf("Failed to upload buffer data\n");
			return NULL;
		}
	}

	return returnBuffer;
}

bool Buffer::copy(Buffer* srcBuffer, Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) {
#if _DEBUG
	if (srcBuffer == NULL || dstBuffer == NULL) {
		printf("Unable to copy NULL buffers\n");
		return false;
	}
	if (srcBuffer->m_device != dstBuffer->m_device) {
		printf("Unable to copy data between buffers on different devices\n");
		return false;
	}
	if (srcOffset + size > srcBuffer->m_size) {
		printf("Copy range in source buffer is out of range\n");
		return false;
	}
	if (dstOffset + size > srcBuffer->m_size) {
		printf("Copy range in destination buffer is out of range\n");
		return false;
	}
#endif
	
	if (size == 0) {
#if _DEBUG
		printf("Buffer::copy was called with size=0, Nothing changed\n");
#endif
		return true;
	}

	vk::Queue transferQueue = **Application::instance()->graphics()->getQueue(QUEUE_TRANSFER_MAIN);
	vk::CommandBuffer transferCommandBuffer = **Application::instance()->graphics()->commandPool().getCommandBuffer("transfer_buffer");

	vk::CommandBufferBeginInfo commandBeginInfo;
	commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	
	transferCommandBuffer.begin(commandBeginInfo);
	vk::BufferCopy copyRegion;
	copyRegion.setSrcOffset(srcOffset);
	copyRegion.setDstOffset(dstOffset);
	copyRegion.setSize(size);
	transferCommandBuffer.copyBuffer(srcBuffer->getBuffer(), dstBuffer->getBuffer(), 1, &copyRegion);
	transferCommandBuffer.end();

	vk::SubmitInfo queueSumbitInfo;
	queueSumbitInfo.setCommandBufferCount(1);
	queueSumbitInfo.setPCommandBuffers(&transferCommandBuffer);
	transferQueue.submit(1, &queueSumbitInfo, VK_NULL_HANDLE);
	transferQueue.waitIdle();

	return true;
}

const vk::Buffer& Buffer::getBuffer() const {
	return m_buffer;
}

vk::DeviceSize Buffer::getSize() const {
	return m_size;
}

bool Buffer::upload(vk::DeviceSize offset, vk::DeviceSize size, void* data) {
#if _DEBUG
	if (offset + size > m_size) {
		printf("Unable to upload data to buffer out of allocated range\n");
		return false;
	}
	if (data == NULL) {
		printf("Unable to upload NULL data to buffer\n");
		return false;
	}
#endif

	if (size == 0) { // 0 bytes uploaded... technically valid
#if _DEBUG
		printf("Buffer::upload was called with size=0. Nothing changed\n");
#endif
		return true;
	}

	if (!(m_memoryProperties & vk::MemoryPropertyFlagBits::eHostVisible)) {
		// This buffer is not visible to the host, so a temporary staging buffer needs to be created, from which the data is copied to this buffer.
		BufferConfiguration stagingBufferConfig;
		stagingBufferConfig.device = m_device;
		stagingBufferConfig.size = size;
		stagingBufferConfig.sharingMode = vk::SharingMode::eExclusive;
		stagingBufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
		stagingBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
		Buffer* stagingBuffer = Buffer::create(stagingBufferConfig);

		if (stagingBuffer == NULL) {
			printf("Unable to upload data to buffer. Failed to create staging buffer\n");
			return false;
		}

		if (!stagingBuffer->upload(0, size, data)) {
			printf("Failed to upload data to staging buffer\n");
			return false;
		}

		if (!Buffer::copy(stagingBuffer, this, size, 0, offset)) {
			printf("Failed to copy data from staging buffer\n");
			delete stagingBuffer;
			return false;
		}

		delete stagingBuffer;
		return true;

	} else {
		void* mappedBuffer = (**m_device).mapMemory(m_deviceMemory, offset, size);
		if (mappedBuffer == NULL) {
			printf("Failed to map buffer memory\n");
			return false;
		}

		memcpy(mappedBuffer, data, size);
		(**m_device).unmapMemory(m_deviceMemory);

		return true;
	}
}
