
#ifndef WORLDENGINE_COMMANDPOOL_H
#define WORLDENGINE_COMMANDPOOL_H

#include "core/core.h"
#include "core/graphics/GraphicsManager.h"

struct CommandPoolConfiguration {
    std::weak_ptr<vkr::Device> device;
    uint32_t queueFamilyIndex;
    bool transient = false; // Hint that command buffers are rerecorded with new commands very often
    bool resetCommandBuffer = false; // Allow command buffers to be rerecorded individually, without this flag they all have to be reset together
};

struct CommandBufferConfiguration {
    vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary;
};

class CommandPool {
    NO_COPY(CommandPool);
private:
    CommandPool(std::weak_ptr<vkr::Device> device, std::unique_ptr<vkr::CommandPool> commandPool);

public:
    ~CommandPool();

    static CommandPool* create(const CommandPoolConfiguration& commandPoolConfiguration);

    const vkr::CommandPool& get() const;

    std::shared_ptr<vkr::CommandBuffer> allocateCommandBuffer(const std::string& name, const CommandBufferConfiguration& commandBufferConfiguration);

    std::shared_ptr<vkr::CommandBuffer> getCommandBuffer(const std::string& name);

    void freeCommandBuffer(const std::string name);

    bool hasCommandBuffer(const std::string& name) const;

    const GraphicsResource& getResourceId() const;

private:
    std::shared_ptr<vkr::Device> m_device;
    std::unique_ptr<vkr::CommandPool> m_commandPool;
    std::unordered_map<std::string, std::shared_ptr<vkr::CommandBuffer>> m_commandBuffers;

    GraphicsResource m_resourceId;
};

#endif //WORLDENGINE_COMMANDPOOL_H
