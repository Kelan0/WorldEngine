#include "CommandPool.h"

CommandPool::CommandPool(std::weak_ptr<vkr::Device> device, std::unique_ptr<vkr::CommandPool> commandPool):
        m_device(device),
        m_commandPool(std::move(commandPool)),
        m_resourceId(GraphicsManager::nextResourceId()) {
}

CommandPool::~CommandPool() {
    for (auto it = m_commandBuffers.begin(); it != m_commandBuffers.end(); ++it) {
        if (it->second.use_count() > 1) {
            printf("Command buffer \"%s\" has %d external references when command pool was destroyed\n", it->first.c_str(), it->second.use_count() - 1);
        }
    }
}

CommandPool* CommandPool::create(const CommandPoolConfiguration& commandPoolConfiguration) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.setQueueFamilyIndex(commandPoolConfiguration.queueFamilyIndex);
    if (commandPoolConfiguration.transient)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    if (commandPoolConfiguration.resetCommandBuffer)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    std::unique_ptr<vkr::CommandPool> commandPool = std::make_unique<vkr::CommandPool>(*commandPoolConfiguration.device.lock(), commandPoolCreateInfo);
    return new CommandPool(commandPoolConfiguration.device, std::move(commandPool));
}

const vkr::CommandPool& CommandPool::get() const {
    return *m_commandPool;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::allocateCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
#if _DEBUG
    if (m_commandBuffers.find(name) != m_commandBuffers.end()) {
		printf("Unable to create command buffer \"%s\", it already exists\n", name.c_str());
		assert(false);
		return NULL;
	}
#endif

    vk::CommandBufferAllocateInfo commandBufferAllocInfo;
    commandBufferAllocInfo.setCommandPool(**m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);
    std::vector<vkr::CommandBuffer> commandBuffers = m_device->allocateCommandBuffers(commandBufferAllocInfo);

    std::shared_ptr<vkr::CommandBuffer> commandBuffer = std::make_shared<vkr::CommandBuffer>(std::move(commandBuffers[0]));

    m_commandBuffers.insert(std::make_pair(name, commandBuffer));

    return commandBuffer;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::getCommandBuffer(const std::string& name) {
    return m_commandBuffers.at(name);
}

void CommandPool::freeCommandBuffer(const std::string name) {
#if _DEBUG
    auto it = m_commandBuffers.find(name);
	if (it == m_commandBuffers.end()) {
		printf("Tried to free command buffer \"%s\" but it was already freed\n", name.c_str());
		return;
	}

	if (it->second.use_count() > 1) {
		printf("Unable to free command buffer \"%s\" because it still has %d references\n", name.c_str(), it->second.use_count() - 1);
		assert(false);
		return;
	}
#endif
    m_commandBuffers.erase(name);
}

bool CommandPool::hasCommandBuffer(const std::string& name) const {
    return m_commandBuffers.count(name) > 0;
}

const GraphicsResource& CommandPool::getResourceId() const {
    return m_resourceId;
}
