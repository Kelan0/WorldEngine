
#ifndef WORLDENGINE_GRAPHICSMANAGER_H
#define WORLDENGINE_GRAPHICSMANAGER_H

#include "core/core.h"
#include "core/graphics/FrameResource.h"
#include "core/util/DebugUtils.h"
#include <SDL2/SDL.h>
#include <SDL_vulkan.h>

#define QUEUE_GRAPHICS_MAIN "graphics_main"
#define QUEUE_COMPUTE_MAIN "compute_main"
#define QUEUE_TRANSFER_MAIN "transfer_main"
#define QUEUE_GRAPHICS_TRANSFER_MAIN "graphics_transfer_main"

//#if 1
//#define INSERT_CMD_LABEL(commandBuffer, label) Engine::graphics()->insertCmdDebugLabel(commandBuffer, label);
//#define BEGIN_CMD_LABEL(commandBuffer, label) Engine::graphics()->beginCmdDebugLabel(commandBuffer, label);
//#define END_CMD_LABEL(commandBuffer) Engine::graphics()->endCmdDebugLabel(commandBuffer);
//#define INSERT_QUEUE_LABEL(queue, label) Engine::graphics()->insertQueueDebugLabel(queue, label);
//#define BEGIN_QUEUE_LABEL(queue, label) Engine::graphics()->beginQueueDebugLabel(queue, label);
//#define END_QUEUE_LABEL(queue) Engine::graphics()->endQueueDebugLabel(queue);
//#else
//#define INSERT_CMD_LABEL(commandBuffer, label)
//#define BEGIN_CMD_LABEL(commandBuffer, label)
//#define END_CMD_LABEL(commandBuffer)
//#define INSERT_QUEUE_LABEL(queue, label)
//#define BEGIN_QUEUE_LABEL(queue, label)
//#define END_QUEUE_LABEL(queue)
//#endif

class RenderPass;
class CommandPool;
class DescriptorPool;
class DeviceMemoryManager;
class DeviceMemoryBlock;
class Framebuffer;
class ImageView;

enum QueueType {
    QueueType_GraphicsBit = VK_QUEUE_GRAPHICS_BIT,
    QueueType_ComputeBit = VK_QUEUE_COMPUTE_BIT,
    QueueType_TransferBit = VK_QUEUE_TRANSFER_BIT,
    QueueType_SparseBindingBit = VK_QUEUE_SPARSE_BINDING_BIT,
    QueueType_ProtectedBit = VK_QUEUE_PROTECTED_BIT,
    QueueType_PresentBit = 0x800,
};

enum SwapchainBufferMode {
    SwapchainBufferMode_DoubleBuffer = 0,
    SwapchainBufferMode_TripleBuffer = 1,
};

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
    vk::PhysicalDeviceProperties physicalDeviceProperties;
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
    std::vector<VkImage> images;
    std::vector<std::shared_ptr<ImageView>> imageViews;
    std::vector<std::shared_ptr<Framebuffer>> framebuffers;
    std::vector<std::shared_ptr<vkr::CommandBuffer>> commandBuffers;
    vk::Extent2D imageExtent;
    std::array<std::unique_ptr<vkr::Semaphore>, CONCURRENT_FRAMES> imageAvailableSemaphores;
    std::array<std::unique_ptr<vkr::Semaphore>, CONCURRENT_FRAMES> renderFinishedSemaphores;
    std::array<std::unique_ptr<vkr::Fence>, CONCURRENT_FRAMES> inFlightFences;
    std::vector<vk::Fence> imagesInFlight;

    uint32_t currentFrameIndex;
    uint32_t currentImageIndex;
    uint32_t prevImageIndex;
};

class GraphicsManager {
    NO_COPY(GraphicsManager)

    friend class Engine;

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

    void flushRendering();

    void presentImageDirect(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& imageLayout);

    const vk::Instance& getInstance() const;

    const std::shared_ptr<vkr::Device>& getDevice() const;

    const vk::PhysicalDevice& getPhysicalDevice() const;

    const vk::PhysicalDeviceMemoryProperties& getDeviceMemoryProperties() const;

    const vk::PhysicalDeviceProperties& getPhysicalDeviceProperties() const;

    const vk::PhysicalDeviceLimits& getPhysicalDeviceLimits() const;

    vk::DeviceSize getAlignedUniformBufferOffset(const vk::DeviceSize& offset);

    uint32_t getPreviousFrameIndex() const;

    const uint32_t& getCurrentFrameIndex() const;

    const vk::CommandBuffer& getCurrentCommandBuffer() const;

    const Framebuffer* getCurrentFramebuffer() const;

    const ImageView* getPreviousFrameImageView() const;

    const std::shared_ptr<vkr::Queue>& getQueue(const std::string& name) const;

    bool hasQueue(const std::string& name) const;

    const uint32_t& getGraphicsQueueFamilyIndex() const;
    const uint32_t& getComputeQueueFamilyIndex() const;
    const uint32_t& getTransferQueueFamilyIndex() const;
    const uint32_t& getSparseBindingQueueFamilyIndex() const;
    const uint32_t& getProtectedQueueFamilyIndex() const;
    const uint32_t& getPresentQueueFamilyIndex() const;

    const vk::CommandBuffer& beginOneTimeCommandBuffer();

    void endOneTimeCommandBuffer(const vk::CommandBuffer& commandBuffer, const vk::Queue& queue);
    void endOneTimeCommandBuffer(const vk::CommandBuffer& commandBuffer, const std::shared_ptr<vkr::Queue>& queue);

    std::shared_ptr<RenderPass> renderPass();

    std::shared_ptr<CommandPool> commandPool();

    std::shared_ptr<DescriptorPool> descriptorPool();

    DeviceMemoryManager& memory();

    glm::ivec2 getResolution() const;

    glm::vec2 getNormalizedPixelSize() const;

    float getAspectRatio() const;

    const vk::Extent2D& getImageExtent() const;

    vk::Format getColourFormat() const;

    vk::Format getDepthFormat() const;

    vk::ColorSpaceKHR getColourSpace() const;

    void setPreferredPresentMode(vk::PresentModeKHR presentMode);

    bool didResolutionChange() const;

    const bool& isDirectImagePresentEnabled() const;

    void setDirectImagePresentEnabled(const bool& directImagePresentEnabled);

    const bool& isSwapchainImageSampled() const;

    void setSwapchainImageSampled(const bool& swapchainImageSampled);

    DebugUtils::RenderInfo& debugInfo();

    const DebugUtils::RenderInfo& getDebugInfo() const;

    static GraphicsResource nextResourceId();

    void setObjectName(const vk::Device& device, const uint64_t& objectHandle, const vk::ObjectType& objectType, const char* objectName);
    void setObjectName(const vk::Device& device, const uint64_t& objectHandle, const vk::ObjectType& objectType, const std::string& objectName);

    void insertQueueDebugLabel(const vk::Queue& queue, const char* name);
    void beginQueueDebugLabel(const vk::Queue& queue, const char* name);
    void endQueueDebugLabel(const vk::Queue& queue);

    void insertCmdDebugLabel(const vk::CommandBuffer& commandBuffer, const char* name);
    void beginCmdDebugLabel(const vk::CommandBuffer& commandBuffer, const char* name);
    void endCmdDebugLabel(const vk::CommandBuffer& commandBuffer);

    bool doAbortOnVulkanError();

    void setAbortOnVulkanError(const bool& abortOnVulkanError);

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

    std::unique_ptr<vkr::DebugUtilsMessengerEXT> m_debugMessenger;

    SwapchainBufferMode m_swapchainBufferMode;
    vk::PresentModeKHR m_preferredPresentMode;
    bool m_isInitialized;
    bool m_recreateSwapchain;
    bool m_resolutionChanged;
    bool m_directImagePresentEnabled;
    bool m_swapchainImageSampled;
    DebugUtils::RenderInfo m_debugInfo;
    bool m_flushRendering;
    bool m_abortOnVulkanError;

    static uint64_t s_nextResourceID;
};

#endif //WORLDENGINE_GRAPHICSMANAGER_H
