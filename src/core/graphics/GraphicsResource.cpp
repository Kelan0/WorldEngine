#include "core/graphics/GraphicsResource.h"
#include "core/graphics/GraphicsManager.h"


GraphicsResource::GraphicsResource(const ResourceType& resourceType, const WeakResource<vkr::Device>& device, const std::string& name):
        m_resourceType(resourceType),
        m_device(device, name),
        m_name(name),
        m_resourceId(GraphicsManager::nextResourceId()) {
}

GraphicsResource::GraphicsResource(GraphicsResource&& move) noexcept :
        m_resourceType(std::exchange(move.m_resourceType, ResourceType_None)),
        m_device(std::exchange(move.m_device, nullptr)),
        m_name(std::exchange(move.m_name, std::string{})),
        m_resourceId(std::exchange(move.m_resourceId, ResourceId{})) {
}

GraphicsResource::~GraphicsResource() = default;

GraphicsResource& GraphicsResource::operator=(GraphicsResource&& move) noexcept {
    m_resourceType = (std::exchange(move.m_resourceType, ResourceType_None));
    m_device = (std::exchange(move.m_device, nullptr));
    m_name = (std::exchange(move.m_name, std::string{}));
    m_resourceId = (std::exchange(move.m_resourceId, ResourceId{}));
    return *this;
}

const SharedResource<vkr::Device>& GraphicsResource::getDevice() const {
    return m_device;
}

const std::string& GraphicsResource::getName() const {
    return m_name;
}

const ResourceId& GraphicsResource::getResourceId() const {
    return m_resourceId;
}

const GraphicsResource::ResourceType& GraphicsResource::getResourceType() const {
    return m_resourceType;
}