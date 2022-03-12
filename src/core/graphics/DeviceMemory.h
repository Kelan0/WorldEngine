#pragma once

#include "../core.h"

class Buffer;
class Image2D;


class DeviceMemoryHeap;
class DeviceMemoryBlock;


class DeviceMemoryManager {
public:

};


struct DeviceMemoryConfiguration {
	std::weak_ptr<vkr::Device> device;
	vk::DeviceSize size;
	vk::MemoryPropertyFlags memoryProperties;
	uint32_t memoryTypeFlags;
};

class DeviceMemoryHeap {
public:
	struct BlockRange {
		vk::DeviceSize offset;
		vk::DeviceSize size;
		bool free;
	};

private:
	DeviceMemoryHeap(std::weak_ptr<vkr::Device> device, vk::DeviceMemory deviceMemory, vk::DeviceSize size);

public:
	DeviceMemoryHeap(vk::DeviceSize size);

	~DeviceMemoryHeap();

	static DeviceMemoryHeap* create(const DeviceMemoryConfiguration& deviceMemoryConfiguration);

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::DeviceMemory& getDeviceMemory();

	vk::DeviceSize getSize() const;

	void bindBufferMemory(const vk::Buffer& buffer, vk::DeviceSize offset) const;

	void bindBufferMemory(const Buffer* buffer, vk::DeviceSize offset) const;

	void bindImageMemory(const vk::Image& image, vk::DeviceSize offset) const;

	void bindImageMemory(const Image2D* image, vk::DeviceSize offset) const;

	static bool selectMemoryType(uint32_t memoryTypeBits, vk::MemoryPropertyFlags memoryPropertyFlags, uint32_t& outMemoryType);

	DeviceMemoryBlock* allocateBlock(vk::DeviceSize size);

	bool freeBlock(DeviceMemoryBlock* block);

private:
	size_t findBlockIndex(vk::DeviceSize size);

	size_t freeBlocksBeginIndex();

	size_t freeBlocksEndIndex();

	size_t allocatedBlocksBeginIndex();

	size_t allocatedBlocksEndIndex();

	bool moveBlock(size_t srcBlockIndex, size_t dstBlockIndex);

	size_t updateBlock(size_t blockIndex, const BlockRange& newBlock);

	void insertBlock(const BlockRange& block);

	size_t getBlockSequenceIndex(const BlockRange& block);

	size_t getBlockSequence(const BlockRange& block);

	void incrementBlockSequences(size_t startIndex, size_t count);

	void decrementBlockSequences(size_t startIndex, size_t count);

	bool isContiguous(size_t firstIndex, size_t secondIndex);

	bool equalBlocks(const BlockRange& lhs, const BlockRange& rhs);
private:
	std::shared_ptr<vkr::Device> m_device;
	vk::DeviceMemory m_deviceMemory;
	vk::DeviceSize m_size;

	vk::DeviceSize m_allocatedRange; // The maximum number of bytes currently allocated. Regions inside this range may be fragmented
	vk::DeviceSize m_allocatedBytes; // The number of bytes currently allocated, excluding fragmented regions.

	std::unordered_set<DeviceMemoryBlock*> m_allocatedBlocks;
	std::vector<BlockRange> m_blocks;
	std::vector<size_t> m_blockSizeSequence;
	size_t m_numFreeBlocks;
	bool m_freeBlocksSorted;
};


class DeviceMemoryBlock {
	friend class DeviceMemoryHeap;
private:
	DeviceMemoryBlock(DeviceMemoryHeap* heap, vk::DeviceSize offset, vk::DeviceSize size);

public:
	~DeviceMemoryBlock();

	const vk::DeviceSize& getOffset() const;

	const vk::DeviceSize& getSize() const;
private:
	DeviceMemoryHeap* m_heap;
	vk::DeviceSize m_offset;
	vk::DeviceSize m_size;
	void* m_mappedPtr;
};

