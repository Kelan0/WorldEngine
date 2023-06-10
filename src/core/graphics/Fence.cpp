
#include "core/graphics/Fence.h"
#include "core/graphics/GraphicsManager.h"
#include "core/application/Engine.h"
#include "core/util/Logger.h"

Fence::Fence(const WeakResource<vkr::Device>& device, const vk::Fence& fence, const std::string& name):
        GraphicsResource(ResourceType_Fence, device, name),
        m_fence(fence) {
}

Fence::~Fence() {
    (**m_device).destroyFence(m_fence);
}

Fence* Fence::create(const FenceConfiguration& fenceConfiguration, const std::string& name) {
    const vk::Device& device = **fenceConfiguration.device.lock(name);

    vk::FenceCreateInfo fenceCreateInfo{};
    if (fenceConfiguration.createSignaled)
        fenceCreateInfo.flags |= vk::FenceCreateFlagBits::eSignaled;

    vk::Fence fence = nullptr;
    vk::Result result = device.createFence(&fenceCreateInfo, nullptr, &fence);

    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Failed to create fence: %s", vk::to_string(result).c_str());
        return nullptr;
    }

    Engine::graphics()->setObjectName(device, (uint64_t)(VkFence)fence, vk::ObjectType::eFence, name);

    return new Fence(fenceConfiguration.device, fence, name);
}

const vk::Fence& Fence::getFence() const {
    return m_fence;
}

Fence::Status Fence::getStatus() const {
    return getFenceStatus(this);
}

bool Fence::reset() const {
    return resetFences(**m_device, &m_fence, 1);
}

bool Fence::wait(uint64_t timeout) const {
    return waitForFences(**m_device, &m_fence, 1, false, timeout);
}

Fence::Status Fence::getFenceStatus(const vk::Device& device, const vk::Fence& fence) {
    vk::Result result = device.getFenceStatus(fence);
    switch (result) {
        case vk::Result::eSuccess:
            return Status_Signaled;
        case vk::Result::eNotReady:
            return Status_NotSignaled;
        case vk::Result::eErrorDeviceLost:
        default:
            return Status_Error;
    }
}

Fence::Status Fence::getFenceStatus(const Fence* fence) {
    return getFenceStatus(**fence->getDevice(), fence->getFence());
}

bool Fence::resetFences(const vk::Device& device, const vk::Fence* fences, uint32_t count) {
    vk::Result result = device.resetFences(count, fences);
    return result == vk::Result::eSuccess;
}

bool Fence::resetFences(Fence* const* fences, uint32_t count) {
    if (count == 0)
        return true;

    const vk::Device& device = **fences[0]->getDevice();
    return resetFences(device, fences, fences + count);
}

bool Fence::waitForFences(const vk::Device& device, const vk::Fence* fences, uint32_t count, bool waitForAll, uint64_t timeout) {
    vk::Result result = device.waitForFences(count, fences, waitForAll, timeout);
    return result == vk::Result::eSuccess;
}

bool Fence::waitForFences(Fence* const* fences, uint32_t count, bool waitForAll, uint64_t timeout) {
    if (count == 0)
        return true;

    const vk::Device& device = **fences[0]->getDevice();
    return waitForFences(device, fences, fences + count, waitForAll, timeout);
}