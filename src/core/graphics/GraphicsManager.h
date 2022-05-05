
#ifndef WORLDENGINE_GRAPHICSMANAGER_H
#define WORLDENGINE_GRAPHICSMANAGER_H

#include "core/core.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/FrameResource.h"
#include "core/util/DebugUtils.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#define QUEUE_GRAPHICS_MAIN "graphics_main"
#define QUEUE_COMPUTE_MAIN "compute_main"
#define QUEUE_TRANSFER_MAIN "transfer_main"

class RenderPass;
class CommandPool;
class DescriptorPool;
class DeviceMemoryManager;
class DeviceMemoryBlock;
class Framebuffer;

struct QueueDetails {
    union {
        struct {
            std::optional<uint32_t> graphicsQueueFamilyIndex;
            std::optional<uint32_t> computeQueueFamilyIndex;
            std::optional<uint32_t> transferQueueFamilyIndex;
            std::optional<uint32_t> sparseBindingQueueFamilyIndex;
            std::optional<uint32_t> protectedQueueFamilyIndex;
            std::optional<uint32_t> presentQueueFamilyIndex;
        } queueFamilies;
        std::array<std::optional<uint32_t>, 6> indices = {};
    };

    std::unordered_map<std::string, std::shared_ptr<vkr::Queue>> queues;

    QueueDetails();
};

struct DeviceDetails {
    std::unique_ptr<vkr::PhysicalDevice> physicalDevice;
    std::shared_ptr<vkr::Device> device;
    vk::PhysicalDeviceMemoryProperties memoryProperties;
};

struct SurfaceDetails {
    std::unique_ptr<vkr::SurfaceKHR> surface;
    vk::SurfaceCapabilitiesKHR capabilities;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentMode;
    vk::Format depthFormat;
};

struct SwapchainDetails {
    std::unique_ptr<vkr::SwapchainKHR> swapchain;
    std::vector<std::shared_ptr<ImageView2D>> imageViews;
    std::vector<std::shared_ptr<Framebuffer>> framebuffers;
    std::vector<std::shared_ptr<vkr::CommandBuffer>> commandBuffers;
    vk::Extent2D imageExtent;
    std::array<std::unique_ptr<vkr::Semaphore>, CONCURRENT_FRAMES> imageAvailableSemaphores;
    std::array<std::unique_ptr<vkr::Semaphore>, CONCURRENT_FRAMES> renderFinishedSemaphores;
    std::array<std::unique_ptr<vkr::Fence>, CONCURRENT_FRAMES> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;

    uint32_t currentFrameIndex;
    uint32_t currentImageIndex;
};

class GraphicsManager {
    NO_COPY(GraphicsManager)

    friend class Application;

private:
    GraphicsManager();

    ~GraphicsManager();

    bool init(SDL_Window* windowHandle, const char* applicationName);

    bool createVulkanInstance(SDL_Window* windowHandle, const char* applicationName);

    bool selectValidationLayers(std::vector<const char*>& layerNames);

    bool createDebugUtilsMessenger();

    bool createSurface(SDL_Window* windowHandle);

    bool comparePhysicalDevices(const vkr::PhysicalDevice& physicalDevice1, const vkr::PhysicalDevice& physicalDevice2) const;

    bool isPhysicalDeviceSuitable(const vkr::PhysicalDevice& physicalDevice) const;

    bool selectQueueFamilies(const vkr::PhysicalDevice& physicalDevice, const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties, uint32_t requiredQueueFlags, QueueDetails& queueFamilyIndices);

    bool selectPhysicalDevice();

    bool createLogicalDevice(std::vector<const char*> const& enabledLayers, std::vector<const char*> const& enabledExtensions, vk::PhysicalDeviceFeatures* deviceFeatures, void* pNext, std::unordered_map<std::string, uint32_t> queueLayout);

    bool initSurfaceDetails();

    bool recreateSwapchain();

    bool createSwapchainImages();

    bool createSwapchainFramebuffers();

    bool createRenderPass();

public:
    bool beginFrame();

    void endFrame();

    const vk::Instance& getInstance() const;

    const std::shared_ptr<vkr::Device>& getDevice() const;

    const vk::PhysicalDevice& getPhysicalDevice() const;

    const vk::PhysicalDeviceMemoryProperties& getDeviceMemoryProperties() const;

    const uint32_t& getCurrentFrameIndex() const;

    const vk::CommandBuffer& getCurrentCommandBuffer() const;

    const Framebuffer* getCurrentFramebuffer() const;

    const std::shared_ptr<vkr::Queue>& getQueue(const std::string& name) const;

    bool hasQueue(const std::string& name) const;

    std::shared_ptr<RenderPass> renderPass();

    std::shared_ptr<CommandPool> commandPool();

    std::shared_ptr<DescriptorPool> descriptorPool();

    DeviceMemoryManager& memory();

    glm::ivec2 getResolution() const;

    float getAspectRatio() const;

    const vk::Extent2D& getImageExtent() const;

    vk::Format getColourFormat() const;

    vk::Format getDepthFormat() const;

    vk::ColorSpaceKHR getColourSpace() const;

    void setPreferredPresentMode(vk::PresentModeKHR presentMode);

    bool didResolutionChange() const;

    DebugUtils::RenderInfo& debugInfo();

    const DebugUtils::RenderInfo& getDebugInfo() const;

    static GraphicsResource nextResourceId();

private:
    vkr::Context m_context;
    std::unique_ptr<vkr::Instance> m_instance;
    DeviceDetails m_device;
    QueueDetails m_queues;
    SurfaceDetails m_surface;
    SwapchainDetails m_swapchain;
    std::shared_ptr<RenderPass> m_renderPass;
    std::shared_ptr<CommandPool> m_commandPool;
    std::shared_ptr<DescriptorPool> m_descriptorPool;
    DeviceMemoryManager* m_memory;

    std::vector<std::function<void()>> m_globalResourceCleanup;

    std::unique_ptr<vkr::DebugUtilsMessengerEXT> m_debugMessenger;

    vk::PresentModeKHR m_preferredPresentMode;
    bool m_isInitialized;
    bool m_recreateSwapchain;
    bool m_resolutionChanged;

    DebugUtils::RenderInfo m_debugInfo;

    static uint64_t s_nextResourceID;
};

#endif //WORLDENGINE_GRAPHICSMANAGER_H
