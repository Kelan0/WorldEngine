
#ifndef WORLDENGINE_DEVICEMEMORY_H
#define WORLDENGINE_DEVICEMEMORY_H

#include "../core.h"

class Buffer;
class Image2D;


class DeviceMemoryHeap;
class DeviceMemoryBlock;


struct DeviceMemoryConfiguration {
    std::weak_ptr<vkr::Device> device;
    vk::DeviceSize size;
    vk::MemoryPropertyFlags memoryProperties;
    uint32_t memoryTypeFlags;
};



class DeviceMemoryManager {
public:
    DeviceMemoryManager(std::weak_ptr<vkr::Device> device);

    ~DeviceMemoryManager();

    static bool selectMemoryType(uint32_t memoryTypeBits, vk::MemoryPropertyFlags memoryPropertyFlags, uint32_t& outMemoryType);

    static vk::DeviceSize getAlignedOffset(vk::DeviceSize offset, vk::DeviceSize alignment);

    DeviceMemoryBlock* allocate(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags memoryProperties);

    void free(DeviceMemoryBlock* block) const;

private:
    DeviceMemoryHeap* getHeap(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags memoryProperties);

private:
    std::unordered_map<uint32_t, std::vector<DeviceMemoryHeap*>> m_memoryHeaps;
    vk::DeviceSize m_heapGenSizeBytes;
    std::shared_ptr<vkr::Device> m_device;
};




class DeviceMemoryHeap {
    friend class DeviceMemoryBlock;

public:
    struct BlockRange {
        vk::DeviceSize offset;
        vk::DeviceSize size;
        bool free;
    };

private:
    DeviceMemoryHeap(const std::weak_ptr<vkr::Device>& device, vk::DeviceMemory deviceMemory, vk::DeviceSize size);

public:
    ~DeviceMemoryHeap();

    static DeviceMemoryHeap* create(const DeviceMemoryConfiguration& deviceMemoryConfiguration);

    std::shared_ptr<vkr::Device> getDevice() const;

    const vk::DeviceMemory& getDeviceMemory();

    vk::DeviceSize getSize() const;

    void bindBufferMemory(const vk::Buffer& buffer, vk::DeviceSize offset) const;

    void bindBufferMemory(const Buffer* buffer, vk::DeviceSize offset) const;

    void bindImageMemory(const vk::Image& image, vk::DeviceSize offset) const;

    void bindImageMemory(const Image2D* image, vk::DeviceSize offset) const;

    DeviceMemoryBlock* allocateBlock(vk::DeviceSize size, vk::DeviceSize alignment);

    bool freeBlock(DeviceMemoryBlock* block);

    vk::DeviceSize getMaxAllocatableSize(vk::DeviceSize alignment = 0) const;

private:
    size_t findBlockIndex(vk::DeviceSize size, vk::DeviceSize alignment);

    size_t freeBlocksBeginIndex();

    size_t freeBlocksEndIndex();

    size_t allocatedBlocksBeginIndex();

    size_t allocatedBlocksEndIndex();

    bool moveBlock(size_t srcBlockIndex, size_t dstBlockIndex);

    size_t updateBlock(size_t blockIndex, const BlockRange& newBlock);

    void insertBlock(const BlockRange& block);

    size_t getBlockSequenceIndex(const BlockRange& block);

    void eraseBlockSequence(size_t index);

    bool isContiguous(size_t firstIndex, size_t secondIndex);

    bool equalBlocks(const BlockRange& lhs, const BlockRange& rhs);

    void map(DeviceMemoryBlock* block);

    void unmap(DeviceMemoryBlock* block);

private:
    std::shared_ptr<vkr::Device> m_device;
    vk::DeviceMemory m_deviceMemory;
    vk::DeviceSize m_size;

    vk::DeviceSize m_allocatedBytes;

    std::unordered_set<DeviceMemoryBlock*> m_allocatedBlocks;
    std::vector<BlockRange> m_blocks;
    std::vector<size_t> m_blockSizeSequence;
    size_t m_numFreeBlocks;

    vk::DeviceSize m_mappedOffset;
    vk::DeviceSize m_mappedSize;
    void* m_mappedPtr;
};


class DeviceMemoryBlock {
    friend class DeviceMemoryHeap;
private:
    DeviceMemoryBlock(DeviceMemoryHeap* heap, vk::DeviceSize offset, vk::DeviceSize size, vk::DeviceSize alignment);

    ~DeviceMemoryBlock();
public:

    DeviceMemoryHeap* getHeap() const;

    const vk::DeviceSize& getOffset() const;

    const vk::DeviceSize& getSize() const;

    void bindBuffer(const vk::Buffer& buffer) const;

    void bindBuffer(const Buffer* buffer) const;

    void bindImage(const vk::Image& image) const;

    void bindImage(const Image2D* image) const;

    bool isMapped() const;

    void* map();

    void unmap();

private:
    DeviceMemoryHeap* m_heap;
    vk::DeviceSize m_offset;
    vk::DeviceSize m_size;
    vk::DeviceSize m_alignment;
    void* m_mappedPtr;
};




DeviceMemoryBlock* vmalloc(vk::MemoryRequirements requirements, vk::MemoryPropertyFlags memoryProperties);

void vfree(DeviceMemoryBlock* memory);

#endif //WORLDENGINE_DEVICEMEMORY_H
