#include "Buffer.h"
#include "GPUMemory.h"
#include "CommandPool.h"
#include "../application/Application.h"

std::unique_ptr<Buffer> Buffer::s_stagingBuffer = NULL;
vk::DeviceSize Buffer::s_maxStagingBufferSize = 128 * 1024 * 1024; // 128 MiB

Buffer::Buffer(std::weak_ptr<vkr::Device> device, vk::Buffer buffer, vk::DeviceMemory deviceMemory, vk::DeviceSize size, vk::MemoryPropertyFlags memoryProperties, GraphicsResource resourceId):
	m_device(device),
	m_buffer(buffer),
	m_deviceMemory(deviceMemory),
	m_size(size),
	m_memoryProperties(memoryProperties), 
	m_resourceId(resourceId) {
	//printf("Create Buffer\n");
}
Buffer::~Buffer() {
	//printf("Destroy Buffer\n");
	(**m_device).destroyBuffer(m_buffer);
	(**m_device).freeMemory(m_deviceMemory);
}

Buffer* Buffer::create(const BufferConfiguration& bufferConfiguration) {
	vk::BufferUsageFlags usage = bufferConfiguration.usage;

	const vk::Device& device = **bufferConfiguration.device.lock();

	vk::Result result;

	if (bufferConfiguration.data != NULL && !(bufferConfiguration.memoryProperties & vk::MemoryPropertyFlagBits::eHostVisible)) {
		// We have data to upload but the buffer is not visible to the host, so we need to allow this 
		// buffer to be a transfer destination, so the data can be copied from a temporary buffer
		usage |= vk::BufferUsageFlagBits::eTransferDst;
	}

	vk::BufferCreateInfo bufferCreateInfo;
	bufferCreateInfo.setUsage(usage);
	bufferCreateInfo.setSize(bufferConfiguration.size);
	bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
	vk::Buffer buffer = VK_NULL_HANDLE;
	result = device.createBuffer(&bufferCreateInfo, NULL, &buffer);

	if (result != vk::Result::eSuccess) {
		printf("Failed to create buffer: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	const vk::MemoryRequirements& memoryRequirements = device.getBufferMemoryRequirements(buffer);

	uint32_t memoryTypeIndex;
	if (!GPUMemory::selectMemoryType(memoryRequirements.memoryTypeBits, bufferConfiguration.memoryProperties, memoryTypeIndex)) {
		device.destroyBuffer(buffer);
		printf("Buffer memory allocation failed\n");
		return NULL;
	}

	vk::MemoryAllocateInfo allocateInfo;
	allocateInfo.setAllocationSize(memoryRequirements.size);
	allocateInfo.setMemoryTypeIndex(memoryTypeIndex);

	vk::DeviceMemory deviceMemory = VK_NULL_HANDLE;
	result = device.allocateMemory(&allocateInfo, NULL, &deviceMemory);

	if (result != vk::Result::eSuccess) {
		device.destroyBuffer(buffer);
		printf("Failed to allocate device memory for buffer: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	device.bindBufferMemory(buffer, deviceMemory, 0);

	Buffer* returnBuffer = new Buffer(bufferConfiguration.device, buffer, deviceMemory, bufferConfiguration.size, bufferConfiguration.memoryProperties, GraphicsManager::nextResourceId());
	
	if (bufferConfiguration.data != NULL) {
		if (!returnBuffer->upload(0, bufferConfiguration.size, bufferConfiguration.data)) {
			printf("Failed to upload buffer data\n");
			delete returnBuffer;
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

	const vk::Queue& transferQueue = **Application::instance()->graphics()->getQueue(QUEUE_TRANSFER_MAIN);
	const vk::CommandBuffer& transferCommandBuffer = **Application::instance()->graphics()->commandPool()->getCommandBuffer("transfer_buffer");

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

bool Buffer::upload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data) {
#if _DEBUG
	if (dstBuffer == NULL) {
		printf("Cannot upload to NULL buffer\n");
		assert(false);
		return false;
	}

	if (data == NULL) {
		printf("Cannot upload NULL data to buffer\n");
		assert(false);
		return false;
	}
	if (offset > dstBuffer->getSize()) {
		printf("Unable to upload data to buffer out of allocated range\n");
		assert(false);
		return false;
	}
#endif

	if (size == VK_WHOLE_SIZE) {
		size = dstBuffer->getSize() - offset;
	}

#if _DEBUG
	if (offset + size > dstBuffer->getSize()) {
		printf("Unable to upload data to buffer out of allocated range\n");
		assert(false);
		return false;
	}
#endif

	if (!dstBuffer->hasMemoryProperties(vk::MemoryPropertyFlagBits::eHostVisible)) {
		return Buffer::stagedUpload(dstBuffer, offset, size, data);
	} else {
		return Buffer::mappedUpload(dstBuffer, offset, size, data);
	}
}

bool Buffer::copyFrom(Buffer* srcBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) {
	return Buffer::copy(srcBuffer, this, size, srcOffset, dstOffset);
}

bool Buffer::copyTo(Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) {
	return Buffer::copy(this, dstBuffer, size, srcOffset, dstOffset);
}

bool Buffer::upload(vk::DeviceSize offset, vk::DeviceSize size, const void* data) {
	return Buffer::upload(this, offset, size, data);
}

std::shared_ptr<vkr::Device> Buffer::getDevice() const {
	return m_device;
}

const vk::Buffer& Buffer::getBuffer() const {
	return m_buffer;
}

vk::DeviceSize Buffer::getSize() const {
	return m_size;
}

vk::MemoryPropertyFlags Buffer::getMemoryProperties() const {
	return m_memoryProperties;
}

bool Buffer::hasMemoryProperties(vk::MemoryPropertyFlags memoryProperties, bool any) {
	if (any) {
		return (uint32_t)(m_memoryProperties & memoryProperties) != 0;
	} else {
		return (m_memoryProperties & memoryProperties) == memoryProperties;
	}
}

const GraphicsResource& Buffer::getResourceId() const {
	return m_resourceId;
}

const std::unique_ptr<Buffer>& Buffer::getStagingBuffer() {
	return s_stagingBuffer;
}

void Buffer::resetStagingBuffer() {
	s_stagingBuffer.reset();
}

bool Buffer::stagedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data) {
	reserveStagingBuffer(dstBuffer->getDevice(), size);

	vk::DeviceSize stageSize = glm::min(s_stagingBuffer->getSize(), size);
	vk::DeviceSize stageOffset = 0;
	const uint8_t* stageData = static_cast<const uint8_t*>(data);

	vk::DeviceSize stages = (size + stageSize - 1) / stageSize; // round-up integer division

	for (vk::DeviceSize i = 0; i < stages; ++i) {
		if (!Buffer::mappedUpload(s_stagingBuffer.get(), 0, stageSize, stageData)) {
			printf("Failed to upload data to staging buffer\n");
			return false;
		}

		if (!Buffer::copy(s_stagingBuffer.get(), dstBuffer, stageSize, 0, offset + stageOffset)) {
			printf("Failed to copy data from staging buffer\n");
			return false;
		}

		stageData += stageSize;
	}

	return true;
}

bool Buffer::mappedUpload(Buffer* dstBuffer, vk::DeviceSize offset, vk::DeviceSize size, const void* data) {
	const vk::Device& device = **dstBuffer->getDevice();
	void* mappedBuffer = NULL;

	vk::Result result = device.mapMemory(dstBuffer->m_deviceMemory, offset, size, {}, &mappedBuffer);
	if (result != vk::Result::eSuccess) {
		printf("Failed to map device memory for buffer: %s\n", vk::to_string(result).c_str());
		return false;
	}
	
	memcpy(mappedBuffer, data, size);
	
	device.unmapMemory(dstBuffer->m_deviceMemory);

	return true;
}

void Buffer::resizeStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size) {
	if (size > s_maxStagingBufferSize) {
		size = s_maxStagingBufferSize;
	}

	std::string startSize = !s_stagingBuffer ? "UNALLOCATED" : std::to_string(s_stagingBuffer->getSize());

	if (s_stagingBuffer) {
		if (size == s_stagingBuffer->getSize()) {
			return; // Do nothing, we are resizing to the same size.
		}

		Buffer::resetStagingBuffer();
	}

	printf("Resizing staging buffer from %s to %llu bytes\n", startSize.c_str(), size);

	BufferConfiguration stagingBufferConfig;
	stagingBufferConfig.device = device;
	stagingBufferConfig.size = size;
	stagingBufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
	stagingBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
	s_stagingBuffer = std::unique_ptr<Buffer>(Buffer::create(stagingBufferConfig));
	assert(!!s_stagingBuffer);
}

void Buffer::reserveStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size) {
	if (!s_stagingBuffer || size > s_stagingBuffer->getSize()) {
		Buffer::resizeStagingBuffer(device, size);
	}
}
