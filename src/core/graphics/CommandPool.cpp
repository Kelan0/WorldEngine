#include "core/graphics/CommandPool.h"

CommandPool::CommandPool(const std::weak_ptr<vkr::Device>& device, const vk::CommandPool& commandPool):
        m_device(device),
        m_commandPool(commandPool),
        m_resourceId(GraphicsManager::nextResourceId()) {
}

CommandPool::~CommandPool() {
    for (auto & commandBuffer : m_commandBuffers) {
        if (commandBuffer.second.use_count() > 1) {
            printf("Command buffer \"%s\" has %llu external references when command pool was destroyed\n", commandBuffer.first.c_str(), (uint64_t)(commandBuffer.second.use_count() - 1));
        }
    }
    m_commandBuffers.clear();
    (**m_device).destroyCommandPool(m_commandPool);
}

CommandPool* CommandPool::create(const CommandPoolConfiguration& commandPoolConfiguration, const char* name) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.setQueueFamilyIndex(commandPoolConfiguration.queueFamilyIndex);
    if (commandPoolConfiguration.transient)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    if (commandPoolConfiguration.resetCommandBuffer)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

//    const std::shared_ptr<vkr::Device> device = commandPoolConfiguration.device.lock();
//    std::unique_ptr<vkr::CommandPool> commandPool = std::make_unique<vkr::CommandPool>(*device, commandPoolCreateInfo);

    const vk::Device& device = **commandPoolConfiguration.device.lock();
    vk::CommandPool commandPool = VK_NULL_HANDLE;
    vk::Result result = device.createCommandPool(&commandPoolCreateInfo, nullptr, &commandPool);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create command pool: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkCommandPool)commandPool, vk::ObjectType::eCommandPool, name);

    return new CommandPool(commandPoolConfiguration.device, commandPool);
}


const vk::CommandPool& CommandPool::getCommandPool() const {
    return m_commandPool;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::allocateCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
#if _DEBUG
    if (m_commandBuffers.find(name) != m_commandBuffers.end()) {
		printf("Unable to create command buffer \"%s\", it already exists\n", name.c_str());
		assert(false);
		return nullptr;
	}
#endif

    vk::CommandBufferAllocateInfo commandBufferAllocInfo;
    commandBufferAllocInfo.setCommandPool(m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);
    std::vector<vkr::CommandBuffer> commandBuffers = m_device->allocateCommandBuffers(commandBufferAllocInfo);

    std::shared_ptr<vkr::CommandBuffer> commandBuffer = std::make_shared<vkr::CommandBuffer>(std::move(commandBuffers[0]));

    Engine::graphics()->setObjectName(**m_device, (uint64_t)(VkCommandBuffer)(**commandBuffer), vk::ObjectType::eCommandBuffer, name.c_str());

    m_commandBuffers.insert(std::make_pair(name, commandBuffer));

    return commandBuffer;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::getOrCreateCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
    if (hasCommandBuffer(name)) {
        return getCommandBuffer(name);
    } else {
        return allocateCommandBuffer(name, commandBufferConfiguration);
    }
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::getCommandBuffer(const std::string& name) {
#if _DEBUG
    assert(m_commandBuffers.contains(name));
#endif
    return m_commandBuffers.at(name);
}

void CommandPool::freeCommandBuffer(const std::string& name) {
#if _DEBUG
    auto it = m_commandBuffers.find(name);
	if (it == m_commandBuffers.end()) {
		printf("Tried to free command buffer \"%s\" but it was already freed\n", name.c_str());
		return;
	}

	if (it->second.use_count() > 1) {
		printf("Unable to free command buffer \"%s\" because it still has %llu references\n", name.c_str(), (uint64_t)(it->second.use_count() - 1));
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
