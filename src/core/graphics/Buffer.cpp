#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/application/Engine.h"
#include "core/engine/scene/event/EventDispatcher.h"

FrameResource<Buffer> Buffer::s_stagingBuffer = nullptr;
vk::DeviceSize Buffer::s_maxStagingBufferSize = 128 * 1024 * 1024; // 128 MiB

Buffer::Buffer(std::weak_ptr<vkr::Device> device, vk::Buffer buffer, DeviceMemoryBlock* memory, vk::DeviceSize size, vk::MemoryPropertyFlags memoryProperties, GraphicsResource resourceId):
        m_device(device),
        m_buffer(buffer),
        m_memory(memory),
        m_size(size),
        m_memoryProperties(memoryProperties),
        m_resourceId(resourceId) {
    //printf("Create Buffer\n");
}
Buffer::~Buffer() {
    //printf("Destroy Buffer\n");
    (**m_device).destroyBuffer(m_buffer);
    vfree(m_memory);
}

Buffer* Buffer::create(const BufferConfiguration& bufferConfiguration) {
    vk::BufferUsageFlags usage = bufferConfiguration.usage;

    const vk::Device& device = **bufferConfiguration.device.lock();

#if _DEBUG
    if (bufferConfiguration.size <= 0) {
        printf("Cannot create zero-size buffer\n");
        assert(false);
        return nullptr;
    }
#endif

    vk::Result result;

    if (bufferConfiguration.data != nullptr && !(bufferConfiguration.memoryProperties & vk::MemoryPropertyFlagBits::eHostVisible)) {
        // We have data to upload but the buffer is not visible to the host, so we need to allow this
        // buffer to be a transfer destination, so the data can be copied from a temporary buffer
        usage |= vk::BufferUsageFlagBits::eTransferDst;
    }

    vk::BufferCreateInfo bufferCreateInfo;
    bufferCreateInfo.setUsage(usage);
    bufferCreateInfo.setSize(bufferConfiguration.size);
    bufferCreateInfo.setSharingMode(vk::SharingMode::eExclusive);
    vk::Buffer buffer = VK_NULL_HANDLE;
    result = device.createBuffer(&bufferCreateInfo, nullptr, &buffer);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create buffer: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    const vk::MemoryRequirements& memoryRequirements = device.getBufferMemoryRequirements(buffer);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, bufferConfiguration.memoryProperties);

    if (memory == nullptr) {
        device.destroyBuffer(buffer);
        printf("Failed to allocate device memory for buffer\n");
        return nullptr;
    }

    memory->bindBuffer(buffer);

    Buffer* returnBuffer = new Buffer(bufferConfiguration.device, buffer, memory, bufferConfiguration.size, bufferConfiguration.memoryProperties, GraphicsManager::nextResourceId());

    if (bufferConfiguration.data != nullptr) {
        if (!returnBuffer->upload(0, bufferConfiguration.size, bufferConfiguration.data)) {
            printf("Failed to upload buffer data\n");
            delete returnBuffer;
            return nullptr;
        }
    }

    return returnBuffer;
}

bool Buffer::copy(Buffer* srcBuffer, Buffer* dstBuffer, vk::DeviceSize size, vk::DeviceSize srcOffset, vk::DeviceSize dstOffset) {
#if _DEBUG
    if (srcBuffer == nullptr || dstBuffer == nullptr) {
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

    const vk::Queue& transferQueue = **Engine::graphics()->getQueue(QUEUE_TRANSFER_MAIN);
    const vk::CommandBuffer& transferCommandBuffer = **Engine::graphics()->commandPool()->getCommandBuffer("transfer_buffer");

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
    if (dstBuffer == nullptr) {
		printf("Cannot upload to NULL buffer\n");
		assert(false);
		return false;
	}

	if (data == nullptr) {
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

void* Buffer::map() {
    return m_memory->map();
}

void Buffer::unmap() {
    m_memory->unmap();
}

bool Buffer::isMapped() const {
    return m_memory->isMapped();
}

const GraphicsResource& Buffer::getResourceId() const {
    return m_resourceId;
}

const Buffer* Buffer::getStagingBuffer() {
    return s_stagingBuffer.get();
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
    if (dstBuffer->m_memory->map() == nullptr) {
        printf("Failed to map device memory for buffer\n");
        return false;
    }

    auto* dstBytes = static_cast<uint8_t*>(dstBuffer->m_memory->map());
    memcpy(dstBytes + offset, data, size);
    dstBuffer->m_memory->unmap();

    return true;
}

void Buffer::resizeStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size) {
    if (size > s_maxStagingBufferSize) {
        size = s_maxStagingBufferSize;
    }

    if (!s_stagingBuffer) {
        Engine::eventDispatcher()->connect(&Buffer::onCleanupGraphics);
    }
    std::string startSize = !s_stagingBuffer ? "UNALLOCATED" : std::to_string(s_stagingBuffer->getSize());

    if (s_stagingBuffer) {
        if (size == s_stagingBuffer->getSize()) {
            return; // Do nothing, we are resizing to the same size.
        }

        s_stagingBuffer.reset();
    }

    printf("Resizing staging buffer from %s to %llu bytes\n", startSize.c_str(), size);

    BufferConfiguration stagingBufferConfig;
    stagingBufferConfig.device = device;
    stagingBufferConfig.size = size;
    stagingBufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    FrameResource<Buffer>::create(s_stagingBuffer, stagingBufferConfig);
    //s_stagingBuffer = std::unique_ptr<Buffer>(Buffer::create(stagingBufferConfig));
    assert(!!s_stagingBuffer);
}

void Buffer::reserveStagingBuffer(std::weak_ptr<vkr::Device> device, vk::DeviceSize size) {
    if (!s_stagingBuffer || size > s_stagingBuffer->getSize()) {
        Buffer::resizeStagingBuffer(device, size);
    }
}

void Buffer::onCleanupGraphics(const ShutdownGraphicsEvent& event) {
    printf("Destroying staging buffer\n");
    s_stagingBuffer.reset();
}
