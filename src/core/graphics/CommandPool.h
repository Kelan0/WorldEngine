
#ifndef WORLDENGINE_COMMANDPOOL_H
#define WORLDENGINE_COMMANDPOOL_H

#include "core/core.h"
#include "core/graphics/GraphicsResource.h"

class Fence;

struct CommandPoolConfiguration {
    WeakResource<vkr::Device> device;
    uint32_t queueFamilyIndex;
    bool transient = false; // Hint that command buffers are rerecorded with new commands very often
    bool resetCommandBuffer = false; // Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
};

struct CommandBufferConfiguration {
    vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary;
};

class CommandPool : public GraphicsResource {
    NO_COPY(CommandPool);
private:
    CommandPool(const WeakResource<vkr::Device>& device, const vk::CommandPool& commandPool, const std::string& name);

public:
    ~CommandPool() override;

    static CommandPool* create(const CommandPoolConfiguration& commandPoolConfiguration, const std::string& name);

    const vk::CommandPool& getCommandPool() const;

    std::shared_ptr<vkr::CommandBuffer> allocateCommandBuffer(const CommandBufferConfiguration& commandBufferConfiguration, const std::string& name);

    std::shared_ptr<vkr::CommandBuffer> allocateNamedCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration);

    std::shared_ptr<vkr::CommandBuffer> getOrCreateNamedCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration);

    std::shared_ptr<vkr::CommandBuffer> getNamedCommandBuffer(const std::string& name);

    const vk::CommandBuffer& getTemporaryCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration);

    Fence* releaseTemporaryCommandBufferFence(const vk::CommandBuffer& commandBuffer);

    void freeCommandBuffer(const std::string& name);

    bool hasCommandBuffer(const std::string& name) const;

private:
    void updateTemporaryCommandBuffers();

    Fence* getFence();

private:
    vk::CommandPool m_commandPool;
    std::vector<std::shared_ptr<vkr::CommandBuffer>> m_unnamedCommandBuffers;
    std::unordered_map<std::string, std::shared_ptr<vkr::CommandBuffer>> m_namedCommandBuffers;
    std::unordered_map<vk::CommandBuffer, Fence*> m_temporaryCmdBufferFences;
    std::vector<Fence*> m_unusedFences;
};

#endif //WORLDENGINE_COMMANDPOOL_H
