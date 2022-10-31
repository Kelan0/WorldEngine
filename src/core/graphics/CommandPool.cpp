#include "core/graphics/CommandPool.h"

CommandPool::CommandPool(const WeakResource<vkr::Device>& device, const vk::CommandPool& commandPool, const std::string& name):
        GraphicsResource(ResourceType_CommandPool, device, name),
        m_commandPool(commandPool) {
}

CommandPool::~CommandPool() {
    for (auto& commandBuffer : m_commandBuffers) {
        if (commandBuffer.second.use_count() > 1) {
            printf("Command buffer \"%s\" has %llu external references when command pool was destroyed\n", commandBuffer.first.c_str(), (uint64_t)(commandBuffer.second.use_count() - 1));
        }
    }

    for (auto& [commandBuffer, fence] : m_temporaryCmdBufferFences) {
        (**m_device).freeCommandBuffers(m_commandPool, 1, &commandBuffer);
        (**m_device).destroyFence(fence);
    }

    for (auto& fence : m_unusedFences) {
        (**m_device).destroyFence(fence);
    }

    m_commandBuffers.clear();
    (**m_device).destroyCommandPool(m_commandPool);
}

CommandPool* CommandPool::create(const CommandPoolConfiguration& commandPoolConfiguration, const std::string& name) {
    vk::CommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.setQueueFamilyIndex(commandPoolConfiguration.queueFamilyIndex);
    if (commandPoolConfiguration.transient)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    if (commandPoolConfiguration.resetCommandBuffer)
        commandPoolCreateInfo.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

//    const std::shared_ptr<vkr::Device> device = commandPoolConfiguration.device.lock();
//    std::unique_ptr<vkr::CommandPool> commandPool = std::make_unique<vkr::CommandPool>(*device, commandPoolCreateInfo);

    const vk::Device& device = **commandPoolConfiguration.device.lock(name);
    vk::CommandPool commandPool = VK_NULL_HANDLE;
    vk::Result result = device.createCommandPool(&commandPoolCreateInfo, nullptr, &commandPool);

    if (result != vk::Result::eSuccess) {
        printf("Failed to create command pool: %s\n", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkCommandPool)commandPool, vk::ObjectType::eCommandPool, name);

    return new CommandPool(commandPoolConfiguration.device, commandPool, name);
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

const vk::CommandBuffer& CommandPool::getTemporaryCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
    const vk::Device& device = **m_device;

    vk::Result result;

    updateTemporaryCommandBuffers();

    vk::Fence fence = VK_NULL_HANDLE;

    if (m_unusedFences.empty()) {
        printf("CommandPool::getTemporaryCommandBuffer - creating fence\n");
        vk::FenceCreateInfo fenceCreateInfo{};
        result = device.createFence(&fenceCreateInfo, nullptr, &fence);
        assert(result == vk::Result::eSuccess && fence);
        Engine::graphics()->setObjectName(device, (uint64_t)(VkFence)fence, vk::ObjectType::eFence, name.c_str());
    } else {
        fence = m_unusedFences.back();
        result = device.resetFences(1, &fence);
        assert(result == vk::Result::eSuccess);
        m_unusedFences.pop_back();
    }

    vk::CommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.setCommandPool(m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);

    vk::CommandBuffer commandBuffer = VK_NULL_HANDLE;
    result = device.allocateCommandBuffers(&commandBufferAllocInfo, &commandBuffer);
    assert(result == vk::Result::eSuccess && commandBuffer);
    Engine::graphics()->setObjectName(device, (uint64_t)(VkCommandBuffer)commandBuffer, vk::ObjectType::eCommandBuffer, name.c_str());

    auto [it, inserted] = m_temporaryCmdBufferFences.insert(std::make_pair(commandBuffer, fence));
    if (!inserted) {
        // This shouldn't happen if the command buffer handle is guaranteed to be unique every time, even from the same pool. Is this the case?
        it->second = fence;
    }
    return it->first;
}

vk::Fence CommandPool::releaseTemporaryCommandBufferFence(const vk::CommandBuffer& commandBuffer) {
    updateTemporaryCommandBuffers();
    auto it = m_temporaryCmdBufferFences.find(commandBuffer);
    if (it == m_temporaryCmdBufferFences.end()) {
        return VK_NULL_HANDLE;
    }
    return it->second;
}

void CommandPool::updateTemporaryCommandBuffers() {
    const vk::Device& device = **m_device;

    vk::Result result;

    for (auto it = m_temporaryCmdBufferFences.begin(); it != m_temporaryCmdBufferFences.end();) {
        const vk::Fence& fence = it->second;
        result = device.waitForFences(1, &fence, VK_FALSE, 0);
        if (result == vk::Result::eSuccess) {
            const vk::CommandBuffer& commandBuffer = it->first;
            device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
            m_unusedFences.emplace_back(fence);
            it = m_temporaryCmdBufferFences.erase(it);
        } else {
            ++it;
        }
    }
}

bool CommandPool::hasCommandBuffer(const std::string& name) const {
    return m_commandBuffers.count(name) > 0;
}
