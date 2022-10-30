#include "core/graphics/Buffer.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/CommandPool.h"
#include "core/application/Engine.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"

FrameResource<Buffer> Buffer::s_stagingBuffer = nullptr;
vk::DeviceSize Buffer::s_maxStagingBufferSize = 128 * 1024 * 1024; // 128 MiB

Buffer::Buffer(const WeakResource<vkr::Device>& device, const vk::Buffer& buffer, DeviceMemoryBlock* memory, const vk::DeviceSize& size, const vk::MemoryPropertyFlags& memoryProperties, const GraphicsResource& resourceId, const std::string& name):
        m_device(device, name),
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

Buffer* Buffer::create(const BufferConfiguration& bufferConfiguration, const std::string& name) {
    vk::BufferUsageFlags usage = bufferConfiguration.usage;

    const vk::Device& device = **bufferConfiguration.device.lock(name);

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

    Engine::graphics()->setObjectName(device, (uint64_t)(VkBuffer)buffer, vk::ObjectType::eBuffer, name);

    const vk::MemoryRequirements& memoryRequirements = device.getBufferMemoryRequirements(buffer);
    DeviceMemoryBlock* memory = vmalloc(memoryRequirements, bufferConfiguration.memoryProperties);

    if (memory == nullptr) {
        device.destroyBuffer(buffer);
        printf("Failed to allocate device memory for buffer\n");
        return nullptr;
    }

    memory->bindBuffer(buffer);

    Buffer* returnBuffer = new Buffer(bufferConfiguration.device, buffer, memory, bufferConfiguration.size, bufferConfiguration.memoryProperties, GraphicsManager::nextResourceId(), name);

    if (bufferConfiguration.data != nullptr) {
        if (!returnBuffer->upload(0, bufferConfiguration.size, bufferConfiguration.data)) {
            printf("Failed to upload buffer data\n");
            delete returnBuffer;
            return nullptr;
        }
    }

    return returnBuffer;
}

bool Buffer::copy(Buffer* srcBuffer, Buffer* dstBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset, const vk::DeviceSize& dstOffset) {
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
    vk::Result result = transferQueue.submit(1, &queueSumbitInfo, VK_NULL_HANDLE);
#if _DEBUG
    assert(result == vk::Result::eSuccess);
#endif
    transferQueue.waitIdle();

    return true;
}

bool Buffer::upload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data, const vk::DeviceSize& srcStride, const vk::DeviceSize& dstStride, const vk::DeviceSize& elementSize) {
#if _DEBUG
    if (dstBuffer == nullptr) {
		printf("Cannot upload to NULL buffer\n");
		assert(false);
		return false;
	}

	if (data == nullptr && size > 0) {
		printf("Cannot upload NULL data to buffer\n");
		assert(false);
		return false;
	}
	if (offset > dstBuffer->getSize()) {
		printf("Unable to upload data to buffer out of allocated range\n");
		assert(false);
		return false;
	}
    if (srcStride != 0 || dstStride != 0) {
        if (elementSize == 0) {
            printf("Unable to upload data to buffer: If srcStride or dstStride is non-zero, elementSize must be non-zero\n");
            assert(false);
            return false;
        }
        if (srcStride != 0) {
            if (srcStride < elementSize) {
                printf("Unable to upload data to buffer: srcStride (%llu bytes) must not be less than elementSize (%llu bytes)\n", srcStride, elementSize);
                assert(false);
                return false;
            }
        }
        if (dstStride != 0) {
            if (dstStride < elementSize) {
                printf("Unable to upload data to buffer: dstStride (%llu bytes) must not be less than elementSize (%llu bytes)\n", dstStride, elementSize);
                assert(false);
                return false;
            }
        }
    }
#endif

    vk::DeviceSize bufferSize = size;
    if (bufferSize == VK_WHOLE_SIZE) {
        // TODO: this is potentially unsafe, and should be disallowed.
        bufferSize = dstBuffer->getSize() - offset;

        if (srcStride != 0 || dstStride != 0) {
            assert(elementSize != 0);
            bufferSize = FLOOR_TO_MULTIPLE(bufferSize, elementSize);
#if _DEBUG
            if (bufferSize == 0) {
                printf("Unable to upload data to buffer: elementSize (%llu bytes) is larger than the remaining buffer size (%llu bytes after offset %llu)\n", elementSize, dstBuffer->getSize() - offset, offset);
                assert(false);
                return false;
            }
#endif
        }
    }

#if _DEBUG
    if (offset + bufferSize > dstBuffer->getSize()) {
		printf("Unable to upload data to buffer out of allocated range\n");
		assert(false);
		return false;
	}
#endif

    if (!dstBuffer->hasMemoryProperties(vk::MemoryPropertyFlagBits::eHostVisible)) {
        return Buffer::stagedUpload(dstBuffer, offset, bufferSize, data, srcStride, dstStride, elementSize);
    } else {
        return Buffer::mappedUpload(dstBuffer, offset, bufferSize, data, srcStride, dstStride, elementSize);
    }
}

bool Buffer::copyFrom(Buffer* srcBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset, const vk::DeviceSize& dstOffset) {
    return Buffer::copy(srcBuffer, this, size, srcOffset, dstOffset);
}

bool Buffer::copyTo(Buffer* dstBuffer, const vk::DeviceSize& size, const vk::DeviceSize& srcOffset, const vk::DeviceSize& dstOffset) {
    return Buffer::copy(this, dstBuffer, size, srcOffset, dstOffset);
}

bool Buffer::upload(const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data, const vk::DeviceSize& srcStride, const vk::DeviceSize& dstStride, const vk::DeviceSize& elementSize) {
    return Buffer::upload(this, offset, size, data, srcStride, dstStride, elementSize);
}

const SharedResource<vkr::Device>& Buffer::getDevice() const {
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

bool Buffer::hasMemoryProperties(const vk::MemoryPropertyFlags& memoryProperties, const bool& any) {
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

bool Buffer::stagedUpload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data, const vk::DeviceSize& srcStride, const vk::DeviceSize& dstStride, const vk::DeviceSize& elementSize) {
    if (size == 0) {
        return true; // We "successfully" uploaded nothing... This is valid
    }

    vk::DeviceSize dstSize = size;
    if (dstStride != 0) {
        assert(elementSize != 0);
        assert(dstStride >= elementSize);
        vk::DeviceSize elementCount = size / elementSize;
        assert(elementCount * elementSize == size); // if elementSize is provided, the size of the data buffer must be a multiple of elementSize
        dstSize = elementCount * dstStride;
    }
    reserveStagingBuffer(dstBuffer->getDevice(), dstSize);

    vk::DeviceSize stageSize = glm::min(s_stagingBuffer->getSize(), dstSize);
    if (dstStride != 0) {
        stageSize = FLOOR_TO_MULTIPLE(stageSize, dstStride);
        assert(stageSize != 0);
    }

    vk::DeviceSize stageOffset = 0;
    const uint8_t* stageData = static_cast<const uint8_t*>(data);


    vk::DeviceSize stages = INT_DIV_CEIL(dstSize, stageSize);

    for (vk::DeviceSize i = 0; i < stages; ++i, stageOffset += stageSize) {
        if (!Buffer::mappedUpload(s_stagingBuffer.get(), 0, stageSize, stageData + stageOffset, srcStride, dstStride, elementSize)) {
            printf("Failed to upload data to staging buffer\n");
            return false;
        }

        if (!Buffer::copy(s_stagingBuffer.get(), dstBuffer, stageSize, 0, offset + stageOffset)) {
            printf("Failed to copy data from staging buffer\n");
            return false;
        }
    }

    return true;
}

bool Buffer::mappedUpload(Buffer* dstBuffer, const vk::DeviceSize& offset, const vk::DeviceSize& size, const void* data, const vk::DeviceSize& srcStride, const vk::DeviceSize& dstStride, const vk::DeviceSize& elementSize) {
    if (size == 0) {
        return true; // We "successfully" uploaded nothing... This is valid
    }
    const vk::Device& device = **dstBuffer->getDevice();
    if (dstBuffer->m_memory->map() == nullptr) {
        printf("Failed to map device memory for buffer\n");
        return false;
    }

    uint8_t* dstBytes = static_cast<uint8_t*>(dstBuffer->m_memory->map());
    const uint8_t* srcBytes = static_cast<const uint8_t*>(data);

    vk::DeviceSize srcOffset = 0;
    vk::DeviceSize dstOffset = offset;

    vk::DeviceSize srcIncr, dstIncr, cpySize;

    if (srcStride != 0 || dstStride != 0) {
        assert(elementSize != 0);
        srcIncr = srcStride == 0 ? elementSize : srcStride;
        dstIncr = dstStride == 0 ? elementSize : dstStride;
        cpySize = elementSize;
    } else {
        srcIncr = dstIncr = cpySize = size;
    }

    while (srcOffset < size) {
        assert(dstOffset + elementSize < dstBuffer->getSize());
        memcpy(dstBytes + dstOffset, srcBytes + srcOffset, cpySize);
        srcOffset += srcIncr;
        dstOffset += dstIncr;
    }

//    memcpy(dstBytes + dstOffset, srcBytes + srcOffset, size);
    dstBuffer->m_memory->unmap();

    return true;
}

void Buffer::resizeStagingBuffer(const WeakResource<vkr::Device>& device, const vk::DeviceSize& size) {
    vk::DeviceSize stagingBufferSize = glm::min(size, s_maxStagingBufferSize);

    if (!s_stagingBuffer) {
        Engine::eventDispatcher()->connect(&Buffer::onCleanupGraphics);
    }
    std::string startSize = !s_stagingBuffer ? "UNALLOCATED" : std::to_string(s_stagingBuffer->getSize());

    if (s_stagingBuffer) {
        if (stagingBufferSize == s_stagingBuffer->getSize()) {
            return; // Do nothing, we are resizing to the same size.
        }

        s_stagingBuffer.reset();
    }

    printf("Resizing staging buffer from %s to %llu bytes\n", startSize.c_str(), stagingBufferSize);

    BufferConfiguration stagingBufferConfig{};
    stagingBufferConfig.device = device;
    stagingBufferConfig.size = stagingBufferSize;
    stagingBufferConfig.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingBufferConfig.memoryProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    FrameResource<Buffer>::create(s_stagingBuffer, stagingBufferConfig, "Buffer-UploadStagingBuffer");
    //s_stagingBuffer = std::unique_ptr<Buffer>(Buffer::create(stagingBufferConfig));
    assert(!!s_stagingBuffer);
}

void Buffer::reserveStagingBuffer(const WeakResource<vkr::Device>& device, const vk::DeviceSize& size) {
    if (!s_stagingBuffer || size > s_stagingBuffer->getSize()) {
        Buffer::resizeStagingBuffer(device, size);
    }
}

void Buffer::onCleanupGraphics(ShutdownGraphicsEvent* event) {
    printf("Destroying staging buffer\n");
    s_stagingBuffer.reset();
}
