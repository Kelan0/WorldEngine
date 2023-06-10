#include "core/graphics/CommandPool.h"
#include "core/graphics/GraphicsManager.h"
#include "core/graphics/Fence.h"
#include "core/application/Engine.h"
#include "core/util/Logger.h"

CommandPool::CommandPool(const WeakResource<vkr::Device>& device, const vk::CommandPool& commandPool, const std::string& name):
        GraphicsResource(ResourceType_CommandPool, device, name),
        m_commandPool(commandPool) {
}

CommandPool::~CommandPool() {
    for (auto& commandBuffer : m_namedCommandBuffers) {
        if (commandBuffer.second.use_count() > 1)
            LOG_WARN("Command buffer \"%s\" has %llu external references when command pool was destroyed", commandBuffer.first.c_str(), (uint64_t)(commandBuffer.second.use_count() - 1));
    }

    size_t cmdBufCount = 0;
    size_t refCount = 0;
    for (auto& commandBuffer : m_unnamedCommandBuffers) {
        if (commandBuffer.use_count() > 1) {
            refCount += commandBuffer.use_count() - 1;
            ++cmdBufCount;
        }
    }
    if (refCount > 0)
        LOG_WARN("%zu unnamed command buffers have %zu external references when command pool was destroyed", cmdBufCount, refCount);

    for (auto& [commandBuffer, fence] : m_temporaryCmdBufferFences) {
        (**m_device).freeCommandBuffers(m_commandPool, 1, &commandBuffer);
        delete fence;
    }

    for (auto& fence : m_unusedFences) {
        delete fence;
    }

    m_namedCommandBuffers.clear();
    m_unnamedCommandBuffers.clear();
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
    vk::CommandPool commandPool = nullptr;
    vk::Result result = device.createCommandPool(&commandPoolCreateInfo, nullptr, &commandPool);

    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create command pool: %s", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkCommandPool)commandPool, vk::ObjectType::eCommandPool, name);

    return new CommandPool(commandPoolConfiguration.device, commandPool, name);
}


const vk::CommandPool& CommandPool::getCommandPool() const {
    return m_commandPool;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::allocateCommandBuffer(const CommandBufferConfiguration& commandBufferConfiguration, const std::string& name) {
    vk::CommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.setCommandPool(m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);
    std::vector<vkr::CommandBuffer> commandBuffers = m_device->allocateCommandBuffers(commandBufferAllocInfo);

    std::shared_ptr<vkr::CommandBuffer> commandBuffer = std::make_shared<vkr::CommandBuffer>(std::move(commandBuffers[0]));

    Engine::graphics()->setObjectName(**m_device, (uint64_t)(VkCommandBuffer)(**commandBuffer), vk::ObjectType::eCommandBuffer, name.c_str());

    m_unnamedCommandBuffers.emplace_back(commandBuffer);

    return commandBuffer;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::allocateNamedCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
#if _DEBUG
    if (m_namedCommandBuffers.find(name) != m_namedCommandBuffers.end()) {
        LOG_FATAL("Unable to create command buffer \"%s\", it already exists", name.c_str());
        assert(false);
        return nullptr;
    }
#endif

    vk::CommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.setCommandPool(m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);
    std::vector<vkr::CommandBuffer> commandBuffers = m_device->allocateCommandBuffers(commandBufferAllocInfo);

    std::shared_ptr<vkr::CommandBuffer> commandBuffer = std::make_shared<vkr::CommandBuffer>(std::move(commandBuffers[0]));

    Engine::graphics()->setObjectName(**m_device, (uint64_t)(VkCommandBuffer)(**commandBuffer), vk::ObjectType::eCommandBuffer, name.c_str());

    m_namedCommandBuffers.insert(std::make_pair(name, commandBuffer));

    return commandBuffer;
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::getOrCreateNamedCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
    if (hasCommandBuffer(name)) {
        return getNamedCommandBuffer(name);
    } else {
        return allocateNamedCommandBuffer(name, commandBufferConfiguration);
    }
}

std::shared_ptr<vkr::CommandBuffer> CommandPool::getNamedCommandBuffer(const std::string& name) {
#if _DEBUG
    assert(m_namedCommandBuffers.contains(name));
#endif
    return m_namedCommandBuffers.at(name);
}

void CommandPool::freeCommandBuffer(const std::string& name) {
#if _DEBUG
    auto it = m_namedCommandBuffers.find(name);
    if (it == m_namedCommandBuffers.end()) {
        LOG_ERROR("Tried to free command buffer \"%s\" but it was already freed", name.c_str());
        return;
    }

    if (it->second.use_count() > 1) {
        LOG_FATAL("Unable to free command buffer \"%s\" because it still has %llu references", name.c_str(), (uint64_t)(it->second.use_count() - 1));
        assert(false);
        return;
    }
#endif
    m_namedCommandBuffers.erase(name);
}

const vk::CommandBuffer& CommandPool::getTemporaryCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration) {
    const vk::Device& device = **m_device;

    vk::Result result;

    updateTemporaryCommandBuffers();

    Fence* fence = getFence();

    vk::CommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.setCommandPool(m_commandPool);
    commandBufferAllocInfo.setCommandBufferCount(1);
    commandBufferAllocInfo.setLevel(commandBufferConfiguration.level);

    vk::CommandBuffer commandBuffer = nullptr;
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

Fence* CommandPool::releaseTemporaryCommandBufferFence(const vk::CommandBuffer& commandBuffer) {
    updateTemporaryCommandBuffers();
    auto it = m_temporaryCmdBufferFences.find(commandBuffer);
    if (it == m_temporaryCmdBufferFences.end()) {
        return nullptr;
    }
    return it->second;
}

void CommandPool::updateTemporaryCommandBuffers() {
    const vk::Device& device = **m_device;

    for (auto it = m_temporaryCmdBufferFences.begin(); it != m_temporaryCmdBufferFences.end();) {
        Fence* fence = it->second;

        if (fence->wait(0)) {
            const vk::CommandBuffer& commandBuffer = it->first;
            device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
            m_unusedFences.emplace_back(fence);
            it = m_temporaryCmdBufferFences.erase(it);
        } else {
            ++it;
        }
    }
}

Fence* CommandPool::getFence() {
    if (m_unusedFences.empty()) {
        FenceConfiguration fenceConfig{};
        fenceConfig.device = m_device;
        return Fence::create(fenceConfig, "CommandPool-Fence");
    } else {
        Fence* fence = m_unusedFences.back();
        m_unusedFences.pop_back();
        fence->reset();
        return fence;
    }
}

bool CommandPool::hasCommandBuffer(const std::string& name) const {
    return m_namedCommandBuffers.count(name) > 0;
}
