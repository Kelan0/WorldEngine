#include "core/graphics/DeviceMemory.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Image2D.h"
#include "core/application/Engine.h"
#include "core/util/Util.h"
#include "core/util/Logger.h"


#define has_no_overlap(b0, b1) (b0.offset < b1.offset ? (b0.offset + b0.size <= b1.offset) : (b1.offset + b1.size <= b0.offset))

#if _DEBUG
#define sanity_check_blocks() sanityCheckBlocks()
#define sanity_check_size_sequence() sanityCheckSizeSequence()
#else
#define sanity_check_blocks()
#define sanity_check_size_sequence()
#endif

struct SizeAlignment {
    vk::DeviceSize size;
    vk::DeviceSize alignment;
};


bool findSmallestFreeBlock(const DeviceMemoryHeap::BlockRange& block, const SizeAlignment& size) {
    if (!block.free) return true; // Consider allocated blocks to have 0 size
    const vk::DeviceSize blockBegin = DeviceMemoryManager::getAlignedOffset(block.offset, size.alignment);
    const vk::DeviceSize blockEnd = block.offset + block.size;
    const vk::DeviceSize blockSize = blockEnd <= blockBegin ? 0 : blockEnd - blockBegin;
    return blockSize < size.size;
}

bool compareFreeBlocks(const DeviceMemoryHeap::BlockRange& lhs, const DeviceMemoryHeap::BlockRange& rhs) {
    return lhs.offset < rhs.offset;

    //if (lhs.size == rhs.size)
    //	return lhs.offset < rhs.offset;
    //return lhs.size < rhs.size;
}

bool compareAllocatedBlocks(const DeviceMemoryHeap::BlockRange& lhs, const DeviceMemoryHeap::BlockRange& rhs) {
    return lhs.offset < rhs.offset;
}

bool compareBlocks(const DeviceMemoryHeap::BlockRange& lhs, const DeviceMemoryHeap::BlockRange& rhs) {
    if (lhs.free != rhs.free)
        return lhs.free; // free blocks sorted before non-free blocks

    if (lhs.free)
        return compareFreeBlocks(lhs, rhs); // Free blocks sorted by size
    else
        return compareAllocatedBlocks(lhs, rhs); // Allocated blocks sorted by offset
}


DeviceMemoryManager::DeviceMemoryManager(const WeakResource<vkr::Device>& device):
        m_device(device, "DeviceMemoryManager-Device"),
        m_heapGenSizeBytes(1024 * 1024 * 128) {
}

DeviceMemoryManager::~DeviceMemoryManager() {
    for (auto& entry : m_memoryHeaps) {
        const std::vector<DeviceMemoryHeap*>& heaps = entry.second;
        for (DeviceMemoryHeap* heap : heaps) {
            delete heap;
        }
    }
    m_memoryHeaps.clear();
}

bool DeviceMemoryManager::selectMemoryType(uint32_t memoryTypeBits, const vk::MemoryPropertyFlags& memoryPropertyFlags, uint32_t& outMemoryType) {

    const vk::PhysicalDeviceMemoryProperties& deviceMemProps = Engine::graphics()->getDeviceMemoryProperties();

    for (uint32_t i = 0; i < deviceMemProps.memoryTypeCount; ++i) {
        if ((memoryTypeBits & (1 << i)) != 0 && (deviceMemProps.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
            outMemoryType = i;
            return true;
        }
    }

    return false;
}

vk::DeviceSize DeviceMemoryManager::getAlignedOffset(vk::DeviceSize offset, vk::DeviceSize alignment) {
    if (alignment <= 1)
        return offset;
    return offset + (alignment - (offset % alignment)) % alignment;
}

DeviceMemoryBlock* DeviceMemoryManager::allocate(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags& memoryProperties, const std::string& name) {
    DeviceMemoryHeap* heap = getHeap(requirements, memoryProperties);
    if (heap == nullptr)
        return nullptr;

    return heap->allocateBlock(requirements.size, requirements.alignment, name);
}

void DeviceMemoryManager::free(DeviceMemoryBlock* block) const {
    DeviceMemoryHeap* heap = block->getHeap();
    heap->freeBlock(block);
}

DeviceMemoryHeap* DeviceMemoryManager::getHeap(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags& memoryProperties) {
    uint32_t memoryTypeIndex;
    if (!selectMemoryType(requirements.memoryTypeBits, memoryProperties, memoryTypeIndex))
        return nullptr;

    auto it0 = m_memoryHeaps.find(memoryTypeIndex);
    if (it0 == m_memoryHeaps.end()) {
        it0 = m_memoryHeaps.insert(std::make_pair(memoryTypeIndex, std::vector<DeviceMemoryHeap*>())).first;
    }

    std::vector<DeviceMemoryHeap*>& heaps = it0->second;

    // Find the heap with the smallest amount of available memory that is still big enough for the required size
    auto it1 = std::upper_bound(heaps.begin(), heaps.end(), requirements, [](const vk::MemoryRequirements& requirements, const DeviceMemoryHeap* heap) {
        return requirements.size < heap->getMaxAllocatableSize(requirements.alignment);
    });

    if (it1 == heaps.end()) {
        DeviceMemoryConfiguration memoryConfiguration;
        memoryConfiguration.device = m_device;
        memoryConfiguration.memoryProperties = memoryProperties;
        memoryConfiguration.memoryTypeFlags = requirements.memoryTypeBits;
        memoryConfiguration.size = INT_DIV_CEIL(requirements.size, m_heapGenSizeBytes) * m_heapGenSizeBytes;

        DeviceMemoryHeap* heap = DeviceMemoryHeap::create(memoryConfiguration, "DeviceMemoryManager-Heap-" + std::to_string(memoryTypeIndex) + "-" + std::to_string(heaps.size()));
        if (heap == nullptr)
            return nullptr;

        heaps.emplace_back(heap);
        return heap;
    }

    return *it1;
}


DeviceMemoryHeap::DeviceMemoryHeap(const WeakResource<vkr::Device>& device, const vk::DeviceMemory& deviceMemory, vk::DeviceSize size, const std::string& name):
        GraphicsResource(ResourceType_DeviceMemoryHeap, device, name),
        m_deviceMemory(deviceMemory),
        m_size(size) {

    m_mappedOffset = 0;
    m_mappedSize = 0;
    m_mappedPtr = nullptr;

    m_numFreeBlocks = 0;
    m_allocatedBytes = 0;
    BlockRange block{};
#if _DEBUG
    block.name = "HeapRootBlock";
#endif
    block.offset = 0;
    block.size = m_size;
    block.free = true;
    insertBlock(block);
}

DeviceMemoryHeap::~DeviceMemoryHeap() {
    const vk::Device& device = **m_device;
    if (m_mappedPtr != nullptr)
        device.unmapMemory(m_deviceMemory);
    device.freeMemory(m_deviceMemory);
}

DeviceMemoryHeap* DeviceMemoryHeap::create(const DeviceMemoryConfiguration& deviceMemoryConfiguration, const std::string& name) {
    assert(!deviceMemoryConfiguration.device.expired());

    char sizeLabel[6] = "";
//    printf("Allocating device memory heap: %.3f %s\n", Util::getMemorySizeMagnitude(deviceMemoryConfiguration.size, sizeLabel), sizeLabel);

    const vk::Device& device = **deviceMemoryConfiguration.device.get();

    uint32_t memoryTypeIndex;

    if (!DeviceMemoryManager::selectMemoryType(deviceMemoryConfiguration.memoryTypeFlags, deviceMemoryConfiguration.memoryProperties, memoryTypeIndex)) {
        LOG_ERROR("Failed to allocate device memory: Memory type 0x%08X was not found with memory properties 0x%08X", deviceMemoryConfiguration.memoryTypeFlags, (uint32_t)deviceMemoryConfiguration.memoryProperties);
        return nullptr;
    }

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.setMemoryTypeIndex(memoryTypeIndex);
    allocInfo.setAllocationSize(deviceMemoryConfiguration.size);

    vk::DeviceMemory deviceMemory = nullptr;
    vk::Result result = device.allocateMemory(&allocInfo, nullptr, &deviceMemory);


    //if (result == vk::Result::eErrorOutOfDeviceMemory || result == vk::Result::eErrorOutOfHostMemory) {
    //	return nullptr;
    //}

    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to allocate device memory: %s", vk::to_string(result).c_str());
        return nullptr;
    }

    return new DeviceMemoryHeap(deviceMemoryConfiguration.device, deviceMemory, deviceMemoryConfiguration.size, name);
}

const vk::DeviceMemory& DeviceMemoryHeap::getDeviceMemory() {
    return m_deviceMemory;
}

vk::DeviceSize DeviceMemoryHeap::getSize() const {
    return m_size;
}

void DeviceMemoryHeap::bindBufferMemory(const vk::Buffer& buffer, vk::DeviceSize offset) const {
    vk::MemoryRequirements bufferRequirements = (**m_device).getBufferMemoryRequirements(buffer);
    assert(offset + bufferRequirements.size <= m_size);
    (**m_device).bindBufferMemory(buffer, m_deviceMemory, offset);
}

void DeviceMemoryHeap::bindBufferMemory(const Buffer* buffer, vk::DeviceSize offset) const {
    bindBufferMemory(buffer->getBuffer(), offset);
}

void DeviceMemoryHeap::bindImageMemory(const vk::Image& image, vk::DeviceSize offset) const {
    (**m_device).bindImageMemory(image, m_deviceMemory, offset);
}

void DeviceMemoryHeap::bindImageMemory(const Image2D* image, vk::DeviceSize offset) const {
    bindImageMemory(image->getImage(), offset);
}

DeviceMemoryBlock* DeviceMemoryHeap::allocateBlock(vk::DeviceSize size, vk::DeviceSize alignment, const std::string& name) {
    if (size == 0)
        return nullptr;

    assert(m_allocatedBytes + size <= m_size);

    size_t parentIndex = findBlockIndex(size, alignment);
    if (parentIndex >= m_blocks.size()) {
        LOG_ERROR("Failed to allocate device memory block of %llu bytes", size);
        return nullptr;
    }

    sanity_check_blocks();

    assert(m_blocks[parentIndex].free);

    BlockRange parentBlock = m_blocks[parentIndex];
    assert(parentBlock.offset + parentBlock.size <= m_size);

    vk::DeviceSize startOffset = DeviceMemoryManager::getAlignedOffset(parentBlock.offset, alignment);
    vk::DeviceSize endOffset = DeviceMemoryManager::getAlignedOffset(startOffset + size, alignment);

    assert(startOffset >= parentBlock.offset);
    assert(startOffset < parentBlock.offset + parentBlock.size);
    assert(endOffset > startOffset);
    assert(endOffset <= parentBlock.offset + parentBlock.size);

    BlockRange block{};
#if _DEBUG
    block.name = name;
#endif
    block.offset = parentBlock.offset;
    block.size = endOffset - parentBlock.offset;
    block.free = false;
    parentBlock.offset += block.size;
    parentBlock.size -= block.size;

//    printf("Allocating GPU heap block [%llu -> %llu]:%llu (%llu bytes)\n", block.offset, block.offset + block.size, alignment, block.size);

    updateBlock(parentIndex, parentBlock);
    insertBlock(block);

    auto* memoryBlock = new DeviceMemoryBlock(this, block.offset, block.size, alignment);
    m_allocatedBlocks.insert(memoryBlock);

    m_allocatedBytes += block.size;

    sanity_check_blocks();

    assert(memoryBlock->getSize() >= size);
    assert(memoryBlock->getOffset() + memoryBlock->getSize() <= m_size);

    return memoryBlock;
}

bool DeviceMemoryHeap::freeBlock(DeviceMemoryBlock* block) {
    if (block == nullptr)
        return false;
    sanity_check_blocks();
//    printf("Deallocating GPU heap block [%llu -> %llu]:%llu (%llu bytes)\n", block->m_offset, block->m_offset + block->m_size, block->m_alignment, block->m_size);

    auto it = m_allocatedBlocks.find(block);
    if (it == m_allocatedBlocks.end()) {
        // block did not exist.
        return false;
    }

    m_allocatedBlocks.erase(it);

    // Find the iterator for the block matching the size and offset of the specified block.
    auto begin = m_blocks.begin() + allocatedBlocksBeginIndex();
    auto end = m_blocks.begin() + allocatedBlocksEndIndex();
    auto it0 = std::lower_bound(begin, end, block->getOffset(), [](const BlockRange& block, const vk::DeviceSize& offset) {
        return block.offset < offset;
    });
    if (it0 == end || it0->offset != block->getOffset() || it0->size != block->getSize()) {
        LOG_FATAL("Failed to free DeviceMemoryBlock [%llu, %llu] - No matching allocation range was found", block->getOffset(), block->getSize());
        assert(false);
        return false;
    }

    block->unmap();

    size_t index = it0 - m_blocks.begin();
    BlockRange newBlock = m_blocks[index];
    assert(!newBlock.free);
    newBlock.free = true;

    m_allocatedBytes -= newBlock.size;

    index = updateBlock(index, newBlock);
    sanity_check_blocks();

    while (index > 0 && m_blocks[index - 1].free) {
        if (!isContiguous(m_blocks[index - 1], m_blocks[index])) {
            assert(has_no_overlap(m_blocks[index - 1], m_blocks[index]));
            break;
        }
        --index;
    }

    size_t endIndex = index;
    while (endIndex + 1 < m_blocks.size() && m_blocks[endIndex + 1].free) {
        if (!isContiguous(m_blocks[endIndex], m_blocks[endIndex + 1])) {
            assert(has_no_overlap(m_blocks[endIndex], m_blocks[endIndex + 1]));
            break;
        }
        ++endIndex;
    }

    newBlock = m_blocks[index];

//    for (size_t i = index + 1; i <= endIndex; ++i) {
//        if(!isContiguous(newBlock, m_blocks[i]) || !has_no_overlap(newBlock, m_blocks[i])) {
//            break;
//        }
//    }

    while (endIndex > index) {
        size_t nextIndex = index + 1;
        assert(isContiguous(newBlock, m_blocks[nextIndex]));
        assert(has_no_overlap(newBlock, m_blocks[nextIndex]));

        if (newBlock.offset >= m_blocks[nextIndex].offset)
            newBlock.offset = m_blocks[nextIndex].offset;
        newBlock.size += m_blocks[nextIndex].size;
        resizeBlock(nextIndex, 0);
        updateBlock(index, newBlock);
        --endIndex;
    }

    sanity_check_blocks();
    return true;
}

vk::DeviceSize DeviceMemoryHeap::getMaxAllocatableSize(vk::DeviceSize alignment) const {
    if (m_blockSizeSequence.empty())
        return 0;

    size_t idx = m_blockSizeSequence[m_blockSizeSequence.size() - 1];
    assert(m_blocks[idx].free);
    vk::DeviceSize size = m_blocks[idx].size;

    if (alignment != 0) {
        size -= (DeviceMemoryManager::getAlignedOffset(m_blocks[idx].offset, alignment) - m_blocks[idx].offset);
    }

    return size;
}

size_t DeviceMemoryHeap::findBlockIndex(vk::DeviceSize size, vk::DeviceSize alignment) {
    // // Returns first free Block with a size greater than or equal to the required size
    // // Assuming m_blocks is sorted, this should be the smallest such block.
    // return std::lower_bound(m_blocks.begin(), m_blocks.end(), size, findSmallestFreeBlock);

    SizeAlignment alignedSize{size, alignment};
    auto it = std::lower_bound(m_blockSizeSequence.begin(), m_blockSizeSequence.end(), alignedSize, [this](size_t blockIndex, const SizeAlignment& alignedSize) {
        return findSmallestFreeBlock(m_blocks[blockIndex], alignedSize);
    });

    if (it == m_blockSizeSequence.end())
        --it;

    if (!m_blocks[*it].free)
        return (size_t)(-1);

    return *it;
}

size_t DeviceMemoryHeap::freeBlocksBeginIndex() {
    return 0;
}

size_t DeviceMemoryHeap::freeBlocksEndIndex() {
    return freeBlocksBeginIndex() + m_numFreeBlocks;
}

size_t DeviceMemoryHeap::allocatedBlocksBeginIndex() {
    return freeBlocksEndIndex();
}

size_t DeviceMemoryHeap::allocatedBlocksEndIndex() {
    return m_blocks.size();
}

bool DeviceMemoryHeap::moveBlock(size_t srcBlockIndex, size_t dstBlockIndex) {
    assert(srcBlockIndex < m_blocks.size() && dstBlockIndex < m_blocks.size());

    if (srcBlockIndex == dstBlockIndex)
        return false;

    bool free = m_blocks[srcBlockIndex].free;

    bool moved = Util::moveIter(m_blocks.begin() + srcBlockIndex, m_blocks.begin() + dstBlockIndex);

    if (moved && free) {
        int incr = srcBlockIndex > dstBlockIndex ? +1 : -1;
        size_t beginIndex = std::min(srcBlockIndex, dstBlockIndex);
        size_t endIndex = std::max(srcBlockIndex, dstBlockIndex);
        for (size_t i = 0; i < m_blockSizeSequence.size(); ++i) {
            if (m_blockSizeSequence[i] == srcBlockIndex) {
                m_blockSizeSequence[i] = dstBlockIndex;

            } else if (m_blockSizeSequence[i] >= beginIndex && m_blockSizeSequence[i] <= endIndex) {
                m_blockSizeSequence[i] += incr;
            }
        }
    }

    sanity_check_size_sequence();

    return moved;
}

size_t DeviceMemoryHeap::resizeBlock(size_t blockIndex, vk::DeviceSize newSize) {
    assert(blockIndex < m_blocks.size());

    BlockRange oldBlock = m_blocks[blockIndex];

    assert(oldBlock.free); // Can only resize free blocks

    sanity_check_size_sequence();

    if (newSize == 0) {
        assert(m_numFreeBlocks == m_blockSizeSequence.size());

        size_t sequenceIndex = getBlockSequenceIndex(oldBlock);
        assert(equalBlocks(m_blocks[m_blockSizeSequence[sequenceIndex]], oldBlock));
        m_blocks.erase(m_blocks.begin() + blockIndex);
        eraseBlockSequence(sequenceIndex);

        assert(m_numFreeBlocks == m_blockSizeSequence.size());

        sanity_check_size_sequence();
        return (size_t)(-1);
    }

    if (oldBlock.size == newSize)
        return blockIndex; // Nothing changed

    //assert(oldBlock.offset + newSize <= m_size);

    BlockRange newBlock = oldBlock;
    newBlock.size = newSize;

    // Move the blockSizeSequence entry accordingly.
    size_t oldSequenceIndex = getBlockSequenceIndex(oldBlock);
    assert(equalBlocks(m_blocks[m_blockSizeSequence[oldSequenceIndex]], oldBlock));

    size_t newSequenceIndex = getBlockSequenceIndex(newBlock);
//    newSequenceIndex = glm::min(newSequenceIndex, m_blockSizeSequence.size() - 1);

    if (newSize > oldBlock.size)
        --newSequenceIndex; // Because the new size has not yet been updated, if size increased, newSequenceIndex will end up after the old index, so needs to be decremented by one to compensate.

//    size_t newSequenceIndex = oldSequenceIndex; // TODO: This can likely be replaced with upper_bound and achieve O(log n) rather than O(n)
//
//    if (newSize > oldBlock.size)
//        while (newSequenceIndex + 1 < m_blockSizeSequence.size() && m_blocks[m_blockSizeSequence[newSequenceIndex + 1]].size < newSize)
//            ++newSequenceIndex;
//    else
//        while (newSequenceIndex > 0 && m_blocks[m_blockSizeSequence[newSequenceIndex - 1]].size > newSize)
//            --newSequenceIndex;

    assert(newSequenceIndex < m_blockSizeSequence.size());
    Util::moveIter(m_blockSizeSequence.begin() + oldSequenceIndex, m_blockSizeSequence.begin() + newSequenceIndex);

    m_blocks[blockIndex] = newBlock;
    sanity_check_size_sequence();
    return blockIndex;
}

size_t DeviceMemoryHeap::updateBlock(size_t blockIndex, const BlockRange& newBlock) {
    BlockRange oldBlock = m_blocks[blockIndex];

    sanity_check_size_sequence();

    if (equalBlocks(oldBlock, newBlock))
        return blockIndex;

    if (newBlock.size != oldBlock.size) {
        if (resizeBlock(blockIndex, newBlock.size) == (size_t)(-1)) {
            return (size_t)(-1);
        }
    }

    m_blocks[blockIndex] = newBlock;

    if (!oldBlock.free && newBlock.free) {
        m_blockSizeSequence.insert(m_blockSizeSequence.begin() + getBlockSequenceIndex(newBlock), blockIndex);
        ++m_numFreeBlocks;

    } else if (oldBlock.free && !newBlock.free) {
        eraseBlockSequence(getBlockSequenceIndex(newBlock));
        --m_numFreeBlocks;
    }

    if (oldBlock.offset != newBlock.offset || oldBlock.free != newBlock.free) {
        auto begin = m_blocks.begin() + (newBlock.free ? freeBlocksBeginIndex() : allocatedBlocksBeginIndex());
        auto end = m_blocks.begin() + (newBlock.free ? freeBlocksEndIndex() : allocatedBlocksEndIndex());
        auto comparator = newBlock.free ? compareFreeBlocks : compareAllocatedBlocks;
        auto dstPos = std::lower_bound(begin, end, newBlock, comparator);
        if (dstPos == end) --dstPos;
        size_t dstIndex = dstPos - m_blocks.begin();
        moveBlock(blockIndex, dstIndex);
        return dstIndex;
    }

    sanity_check_size_sequence();

    return blockIndex;
}

void DeviceMemoryHeap::insertBlock(const BlockRange& block) {
    auto begin = m_blocks.begin() + (block.free ? freeBlocksBeginIndex() : allocatedBlocksBeginIndex());
    auto end = m_blocks.begin() + (block.free ? freeBlocksEndIndex() : allocatedBlocksEndIndex());
    auto insertPos = m_blocks.insert(std::upper_bound(begin, end, block, compareAllocatedBlocks), block);

    if (block.free) {
        size_t insertIndex = insertPos - m_blocks.begin();
        m_blockSizeSequence.insert(m_blockSizeSequence.begin() + getBlockSequenceIndex(block), insertIndex);
        ++m_numFreeBlocks;
        assert(m_blockSizeSequence.size() == m_numFreeBlocks);
    }
}

size_t DeviceMemoryHeap::getBlockSequenceIndex(const BlockRange& block) {
    if (!block.free)
        return (size_t)(-1);

    // Find first element where comparator returns true
    auto it = std::upper_bound(m_blockSizeSequence.begin(), m_blockSizeSequence.end(), block, [this](const BlockRange& block, size_t blockIndex) {
        assert(blockIndex < m_blocks.size());
        if (block.size == m_blocks[blockIndex].size)
            return block.offset <= m_blocks[blockIndex].offset;
        return block.size < m_blocks[blockIndex].size;
    });

    return it - m_blockSizeSequence.begin();
}

void DeviceMemoryHeap::insertBlockSequence(size_t index) {

}

void DeviceMemoryHeap::eraseBlockSequence(size_t index) {
    assert(index < m_blockSizeSequence.size());

    size_t sequence = m_blockSizeSequence[index];
    m_blockSizeSequence.erase(m_blockSizeSequence.begin() + index);
    for (size_t i = 0; i < m_blockSizeSequence.size(); ++i)
        if (m_blockSizeSequence[i] > sequence)
            --m_blockSizeSequence[i];
    --m_numFreeBlocks;

    sanity_check_size_sequence();
    assert(m_blockSizeSequence.size() == m_numFreeBlocks);
}

bool DeviceMemoryHeap::isContiguous(const BlockRange& firstBlock, const BlockRange& secondBlock) {
    if (firstBlock.offset < secondBlock.offset) {
        return firstBlock.offset + firstBlock.size == secondBlock.offset;
    } else {
        return secondBlock.offset + secondBlock.size == firstBlock.offset;
    }
}

bool DeviceMemoryHeap::equalBlocks(const BlockRange& lhs, const BlockRange& rhs) {
    return lhs.offset == rhs.offset && lhs.size == rhs.size && lhs.free == rhs.free;
}

void DeviceMemoryHeap::map(DeviceMemoryBlock* block) {
    if (block->m_heap != this)
        return;

    if (block->isMapped())
        return;

    const vk::Device& device = **m_device;

    if (m_mappedPtr == nullptr) {

        m_mappedOffset = 0;
        m_mappedSize = m_size;

        m_mappedPtr = device.mapMemory(m_deviceMemory, m_mappedOffset, m_mappedSize);
        if (m_mappedPtr == nullptr)
            return;
    }

    const vk::DeviceSize offset = DeviceMemoryManager::getAlignedOffset(block->m_offset, block->m_alignment);
    block->m_mappedPtr = static_cast<char*>(m_mappedPtr) + (offset - m_mappedOffset);
}

void DeviceMemoryHeap::unmap(DeviceMemoryBlock* block) {
    if (!block->isMapped())
        return;

    block->m_mappedPtr = nullptr;

    // We leave the memory of this heap mapped permenantly for now.
    // TODO: avoid map/unmap wherever possible - Keep frequently accessed regions mapped

    //m_mappedBlocks.erase(block);
    //if (m_mappedBlocks.size() == 0) {
    //	const vk::Device& device = **m_device;
    //	device.unmapMemory(m_deviceMemory);
    //	m_mappedPtr = nullptr;
    //	m_mappedOffset = 0;
    //	m_mappedSize = 0;
    //	block->m_mappedPtr = nullptr;
    //}
}

#if _DEBUG
void DeviceMemoryHeap::sanityCheckBlocks() {
    // If we need to use this method, things are going pretty bad... good luck :(

    vk::DeviceSize freeBlockSizeSum = 0;
    vk::DeviceSize allocatedBlockSizeSum = 0;
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i].free) {
            freeBlockSizeSum += m_blocks[i].size;
        } else {
            allocatedBlockSizeSum += m_blocks[i].size;
        }

        assert(m_blocks[i].offset + m_blocks[i].size <= m_size);

        if (i > 0) {
//            assert(isContiguous(m_blocks[i - 1], m_blocks[i]));
            assert(has_no_overlap(m_blocks[i - 1], m_blocks[i]));
        }
    }

    assert(allocatedBlockSizeSum == m_allocatedBytes);
    assert((freeBlockSizeSum + allocatedBlockSizeSum) == m_size);

    sanityCheckSizeSequence();
}

void DeviceMemoryHeap::sanityCheckSizeSequence() {
    assert(m_blockSizeSequence.size() == m_numFreeBlocks);
    assert(m_numFreeBlocks <= m_blocks.size());

    for (size_t i = 0; i < m_blockSizeSequence.size(); ++i) {
        assert(m_blocks[m_blockSizeSequence[i]].free);
        if (i > 0) {
            if (m_blocks[m_blockSizeSequence[i - 1]].size == m_blocks[m_blockSizeSequence[i]].size) {
                assert(m_blocks[m_blockSizeSequence[i - 1]].offset <= m_blocks[m_blockSizeSequence[i]].offset);
            } else {
                assert(m_blocks[m_blockSizeSequence[i - 1]].size < m_blocks[m_blockSizeSequence[i]].size);
            }
        }
    }
}
#endif

DeviceMemoryBlock::DeviceMemoryBlock(DeviceMemoryHeap* heap, vk::DeviceSize offset, vk::DeviceSize size, vk::DeviceSize alignment):
        m_heap(heap),
        m_offset(offset),
        m_size(size),
        m_alignment(alignment),
        m_mappedPtr(nullptr) {
}

DeviceMemoryBlock::~DeviceMemoryBlock() = default;

vk::DeviceSize DeviceMemoryBlock::getOffset() const {
    return m_offset;
}

vk::DeviceSize DeviceMemoryBlock::getSize() const {
    return m_size;
}

void DeviceMemoryBlock::bindBuffer(const vk::Buffer& buffer) const {
    m_heap->bindBufferMemory(buffer, DeviceMemoryManager::getAlignedOffset(m_offset, m_alignment));
}

void DeviceMemoryBlock::bindBuffer(const Buffer* buffer) const {
    m_heap->bindBufferMemory(buffer, DeviceMemoryManager::getAlignedOffset(m_offset, m_alignment));
}

void DeviceMemoryBlock::bindImage(const vk::Image& image) const {
    m_heap->bindImageMemory(image, DeviceMemoryManager::getAlignedOffset(m_offset, m_alignment));
}

void DeviceMemoryBlock::bindImage(const Image2D* image) const {
    m_heap->bindImageMemory(image, DeviceMemoryManager::getAlignedOffset(m_offset, m_alignment));
}

bool DeviceMemoryBlock::isMapped() const {
    return m_mappedPtr != nullptr;
}

void* DeviceMemoryBlock::map() {
    m_heap->map(this);
    return m_mappedPtr;
}

void DeviceMemoryBlock::unmap() {
    m_heap->unmap(this);
}

DeviceMemoryHeap* DeviceMemoryBlock::getHeap() const {
    return m_heap;
}


DeviceMemoryBlock* vmalloc(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags& memoryProperties, const std::string& name) {
    return Engine::graphics()->memory().allocate(requirements, memoryProperties, name);
}

void vfree(DeviceMemoryBlock* memory) {
    return Engine::graphics()->memory().free(memory);
}
