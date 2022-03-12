#include "DeviceMemory.h"
#include "../application/Application.h"
#include "GraphicsManager.h"
#include "Buffer.h"
#include "Image.h"
#include "../util/Util.h"

constexpr vk::DeviceSize MAX_ADDR = std::numeric_limits<vk::DeviceSize>::max();

bool findSmallestFreeBlock(const vk::DeviceSize& size, const DeviceMemoryHeap::BlockRange& block) {
	if (!block.free)
		return false;
	return size < block.size;
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


DeviceMemoryHeap::DeviceMemoryHeap(std::weak_ptr<vkr::Device> device, vk::DeviceMemory deviceMemory, vk::DeviceSize size):
	m_device(device),
	m_deviceMemory(deviceMemory),
	m_size(size) {

	m_allocatedRange = 0;
	m_allocatedBytes = 0;
	BlockRange block;
	block.offset = 0;
	block.size = m_size;
	block.free = true;
	insertBlock(block);
}

DeviceMemoryHeap::DeviceMemoryHeap(vk::DeviceSize size) {
	m_size = (size);
	m_allocatedRange = 0;
	m_allocatedBytes = 0;
	BlockRange block;
	block.offset = 0;
	block.size = m_size;
	block.free = true;
	insertBlock(block);
}

DeviceMemoryHeap::~DeviceMemoryHeap() {
	(**m_device).freeMemory(m_deviceMemory);
}

DeviceMemoryHeap* DeviceMemoryHeap::create(const DeviceMemoryConfiguration& deviceMemoryConfiguration) {
	assert(!deviceMemoryConfiguration.device.expired());

	const vk::Device& device = **deviceMemoryConfiguration.device.lock();

	uint32_t memoryTypeIndex;

	if (!selectMemoryType(deviceMemoryConfiguration.memoryTypeFlags, deviceMemoryConfiguration.memoryProperties, memoryTypeIndex)) {
		printf("Failed to allocate device memory: Memory type 0x%08X was not found with memory properties 0x%08X\n", deviceMemoryConfiguration.memoryTypeFlags, (uint32_t)deviceMemoryConfiguration.memoryProperties);
		return NULL;
	}

	vk::MemoryAllocateInfo allocInfo;
	allocInfo.setMemoryTypeIndex(memoryTypeIndex);
	allocInfo.setAllocationSize(deviceMemoryConfiguration.size);

	vk::DeviceMemory deviceMemory = VK_NULL_HANDLE;
	vk::Result result = device.allocateMemory(&allocInfo, NULL, &deviceMemory);


	//if (result == vk::Result::eErrorOutOfDeviceMemory || result == vk::Result::eErrorOutOfHostMemory) {
	//	return NULL;
	//}

	if (result != vk::Result::eSuccess) {
		printf("Failed to allocate device memory: %s\n", vk::to_string(result).c_str());
		return NULL;
	}

	return new DeviceMemoryHeap(deviceMemoryConfiguration.device, deviceMemory, deviceMemoryConfiguration.size);
}

std::shared_ptr<vkr::Device> DeviceMemoryHeap::getDevice() const {
	return m_device;
}

const vk::DeviceMemory& DeviceMemoryHeap::getDeviceMemory() {
	return m_deviceMemory;
}

vk::DeviceSize DeviceMemoryHeap::getSize() const {
	return m_size;
}

void DeviceMemoryHeap::bindBufferMemory(const vk::Buffer& buffer, vk::DeviceSize offset) const {
	vk::MemoryRequirements bufferRequirements = (**m_device).getBufferMemoryRequirements(buffer);
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

bool DeviceMemoryHeap::selectMemoryType(uint32_t memoryTypeBits, vk::MemoryPropertyFlags memoryPropertyFlags, uint32_t& outMemoryType) {

	const vk::PhysicalDeviceMemoryProperties& deviceMemProps = Application::instance()->graphics()->getDeviceMemoryProperties();

	for (uint32_t i = 0; i < deviceMemProps.memoryTypeCount; ++i) {
		if ((memoryTypeBits & (1 << i)) != 0 && (deviceMemProps.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags) {
			outMemoryType = i;
			return true;
		}
	}

	return false;
}

DeviceMemoryBlock* DeviceMemoryHeap::allocateBlock(vk::DeviceSize size) {
	if (size == 0)
		return NULL;

	size_t parentIndex = findBlockIndex(size);
	if (parentIndex >= m_blocks.size()) {
		printf("Failed to allocate device memory block of %llu bytes\n", size);
		return NULL;
	}

	assert(m_blocks[parentIndex].free);

	BlockRange parentBlock = m_blocks[parentIndex];
	BlockRange block;
	block.offset = parentBlock.offset;
	block.size = size;
	block.free = false;
	parentBlock.offset += size;
	parentBlock.size -= size;

	updateBlock(parentIndex, parentBlock);
	insertBlock(block);

	DeviceMemoryBlock* memoryBlock = new DeviceMemoryBlock(this, block.offset, block.size);
	m_allocatedBlocks.insert(memoryBlock);

	m_allocatedRange = glm::max(m_allocatedRange, block.offset + block.size);
	m_allocatedBytes += size;

	return memoryBlock;
}

bool DeviceMemoryHeap::freeBlock(DeviceMemoryBlock* block) {
	if (block == NULL)
		return false;

	if (m_allocatedBlocks.erase(block) == 0) {
		// block did not exist.
		return false;
	}

	// Find the iterator for the block matching the size and offset of the specified block.
	auto begin = m_blocks.begin() + allocatedBlocksBeginIndex();
	auto end = m_blocks.begin() + allocatedBlocksEndIndex();
	auto it0 = std::lower_bound(begin, end, block->getOffset(), [](const BlockRange& block, const vk::DeviceSize& offset) {
		return block.offset < offset;
	});
	if (it0 == end || it0->offset != block->getOffset() || it0->size != block->getSize()) {
		printf("Failed to free DeviceMemoryBlock [%llu, %llu] - No matching allocation range was found\n", block->getOffset(), block->getSize());
		assert(false);
		return false;
	}

	size_t index = it0 - m_blocks.begin();
	BlockRange newBlock = m_blocks[index];
	newBlock.free = true;

	index = updateBlock(index, newBlock);

	while (index > 0 && m_blocks[index - 1].free) {
		if (!isContiguous(index - 1, index)) break;
		--index;
	}

	size_t endIndex = index;
	while (endIndex < m_blocks.size() && m_blocks[endIndex + 1].free) {
		if (!isContiguous(endIndex, endIndex + 1)) break;
		++endIndex;
	}

	newBlock = m_blocks[index];
	while (endIndex != index) {
		newBlock.size += m_blocks[index + 1].size;
		updateBlock(index + 1, BlockRange{.size = 0});
		--endIndex;
	}

	updateBlock(index, newBlock);
}

size_t DeviceMemoryHeap::findBlockIndex(vk::DeviceSize size) {
	// // Returns first free Block with a size greater than or equal to the required size
	// // Assuming m_blocks is sorted, this should be the smallest such block.
	// return std::lower_bound(m_blocks.begin(), m_blocks.end(), size, findSmallestFreeBlock);


	auto it = std::lower_bound(m_blockSizeSequence.begin(), m_blockSizeSequence.end(), size, [this](const size_t& blockIndex, vk::DeviceSize size) {
		const BlockRange& block = m_blocks[blockIndex];
		if (!block.free) return true; // Consider allocated blocks to have 0 size
		return block.size < size;
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

	return moved;
}

size_t DeviceMemoryHeap::updateBlock(size_t blockIndex, const BlockRange& newBlock) {
	BlockRange oldBlock = m_blocks[blockIndex];

	if (newBlock.size == 0) {

		if (oldBlock.free) {
			size_t startIndex = getBlockSequenceIndex(oldBlock);
			m_blockSizeSequence.erase(m_blockSizeSequence.begin() + startIndex);
			decrementBlockSequences(startIndex, m_blockSizeSequence.size() - startIndex);
			--m_numFreeBlocks;
		}

		m_blocks.erase(m_blocks.begin() + blockIndex);

		return (size_t)(-1);

	} else {
		if (equalBlocks(oldBlock, newBlock))
			return blockIndex;

		m_blocks[blockIndex] = newBlock;

		size_t dstIndex = blockIndex;
		auto begin = m_blocks.begin() + (newBlock.free ? freeBlocksBeginIndex() : allocatedBlocksBeginIndex());
		auto end = m_blocks.begin() + (newBlock.free ? freeBlocksEndIndex() : allocatedBlocksEndIndex());

		if (!oldBlock.free && newBlock.free) {
			m_blockSizeSequence.insert(m_blockSizeSequence.begin() + getBlockSequenceIndex(newBlock), dstIndex);
			++m_numFreeBlocks;

		} else if (oldBlock.free && !newBlock.free) {
			m_blockSizeSequence.erase(m_blockSizeSequence.begin() + getBlockSequenceIndex(newBlock));
			--m_numFreeBlocks;
		}


		if (oldBlock.offset != newBlock.offset || oldBlock.free != newBlock.free) {
			auto comparator = newBlock.free ? compareFreeBlocks : compareAllocatedBlocks;
			auto dstPos = std::lower_bound(begin, end, newBlock, comparator);
			if (dstPos == end) --dstPos;
			dstIndex = dstPos - m_blocks.begin();
			moveBlock(blockIndex, dstIndex);
		}
		return dstIndex;
	}
}

void DeviceMemoryHeap::insertBlock(const BlockRange& block) {
	auto begin = m_blocks.begin() + (block.free ? freeBlocksBeginIndex() : allocatedBlocksBeginIndex());
	auto end = m_blocks.begin() + (block.free ? freeBlocksEndIndex() : allocatedBlocksEndIndex());
	auto insertPos = m_blocks.insert(std::upper_bound(begin, end, block, compareAllocatedBlocks), block);
	size_t insertIndex = insertPos - m_blocks.begin();

	if (block.free) {
		m_blockSizeSequence.insert(m_blockSizeSequence.begin() + getBlockSequenceIndex(block), insertIndex);
		++m_numFreeBlocks;
	}
}

size_t DeviceMemoryHeap::getBlockSequenceIndex(const BlockRange& block) {
	if (!block.free)
		return (size_t)(-1);

	auto it = std::lower_bound(m_blockSizeSequence.begin(), m_blockSizeSequence.end(), block, [this](const size_t& blockIndex, const BlockRange& block) {
		if (m_blocks[blockIndex].size == block.size)
			return m_blocks[blockIndex].offset < block.offset;
		return m_blocks[blockIndex].size < block.size;
	});

	return it - m_blockSizeSequence.begin();
}

size_t DeviceMemoryHeap::getBlockSequence(const BlockRange& block) {
	size_t index = getBlockSequenceIndex(block);
	if (index == (size_t)(-1))
		return (size_t)(-1);
	return m_blockSizeSequence[index];
}

void DeviceMemoryHeap::incrementBlockSequences(size_t startIndex, size_t count) {
	for (size_t i = 0; i < count; ++i)
		++m_blockSizeSequence[startIndex + i];
}

void DeviceMemoryHeap::decrementBlockSequences(size_t startIndex, size_t count) {
	for (size_t i = 0; i < count; ++i)
		--m_blockSizeSequence[startIndex + i];
}

bool DeviceMemoryHeap::isContiguous(size_t firstIndex, size_t secondIndex) {
	const BlockRange& b0 = m_blocks[firstIndex];
	const BlockRange& b1 = m_blocks[secondIndex];
	return b0.offset + b0.size == b1.offset;
}

bool DeviceMemoryHeap::equalBlocks(const BlockRange& lhs, const BlockRange& rhs) {
	return lhs.offset == rhs.offset && lhs.size == rhs.size && lhs.free == rhs.free;
}


DeviceMemoryBlock::DeviceMemoryBlock(DeviceMemoryHeap* heap, vk::DeviceSize offset, vk::DeviceSize size):
	m_heap(heap),
	m_offset(offset),
	m_size(size),
	m_mappedPtr(NULL) {
}

const vk::DeviceSize& DeviceMemoryBlock::getOffset() const {
	return m_offset;
}

const vk::DeviceSize& DeviceMemoryBlock::getSize() const {
	return m_size;
}

DeviceMemoryBlock::~DeviceMemoryBlock() {
	m_heap->freeBlock(this);
}