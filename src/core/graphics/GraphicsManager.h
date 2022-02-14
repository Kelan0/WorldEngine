#pragma once

#include "../core.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL.h>
#include <SDL_vulkan.h>
#include "GraphicsPipeline.h"
#include "DescriptorSet.h"
#include "Image.h"

#define QUEUE_GRAPHICS_MAIN "graphics_main"
#define QUEUE_COMPUTE_MAIN "compute_main"
#define QUEUE_TRANSFER_MAIN "transfer_main"

class GraphicsPipeline;
class CommandPool;
class GPUMemory;
class DescriptorAllocator;
class DescriptorLayoutCache;

struct QueueDetails {
	union {
		struct {
			std::optional<uint32_t> graphicsQueueFamilyIndex;
			std::optional<uint32_t> computeQueueFamilyIndex;
			std::optional<uint32_t> transferQueueFamilyIndex;
			std::optional<uint32_t> sparseBindingQueueFamilyIndex;
			std::optional<uint32_t> protectedQueueFamilyIndex;
			std::optional<uint32_t> presentQueueFamilyIndex;
		};
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
};

struct SwapchainDetails {
	std::unique_ptr<vkr::SwapchainKHR> swapchain;
	std::vector<std::shared_ptr<ImageView2D>> imageViews;
	std::shared_ptr<Image2D> depthImage;
	std::shared_ptr<ImageView2D> depthImageView;
	std::vector<std::shared_ptr<vkr::Framebuffer>> framebuffers;
	std::vector<std::shared_ptr<vkr::CommandBuffer>> commandBuffers;
	vk::Extent2D imageExtent;
	std::vector<std::unique_ptr<vkr::Semaphore>> imageAvailableSemaphores;
	std::vector<std::unique_ptr<vkr::Semaphore>> renderFinishedSemaphores;
	std::vector<std::unique_ptr<vkr::Fence>> inFlightFences;
	std::vector<vk::Fence> imagesInFlight;

	uint32_t maxFramesInFlight;
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

	bool createLogicalDevice(std::vector<const char*> const& enabledLayers, std::vector<const char*> const& enabledExtensions, vk::PhysicalDeviceFeatures* enabledFeatures, std::unordered_map<std::string, uint32_t> queueLayout);

	bool initSurfaceDetails();

	bool recreateSwapchain();

	bool createSwapchainImages();

	bool createSwapchainFramebuffers();
public:

	bool beginFrame();

	void endFrame();

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::PhysicalDevice& getPhysicalDevice() const;

	const vk::PhysicalDeviceMemoryProperties& getDeviceMemoryProperties() const;

	const vk::CommandBuffer& getCurrentCommandBuffer() const;

	const vk::Framebuffer& getCurrentFramebuffer() const;

	std::shared_ptr<vkr::Queue> getQueue(const std::string& name) const;

	bool hasQueue(const std::string& name) const;

	GraphicsPipeline& pipeline();

	CommandPool& commandPool();

	GPUMemory& gpuMemory();

	glm::ivec2 getResolution() const;

	float getAspectRatio() const;

	const vk::Extent2D& getImageExtent() const;

	vk::Format getColourFormat() const;

	vk::Format getDepthFormat() const;

	vk::ColorSpaceKHR getColourSpace() const;

	void initializeGraphicsPipeline(const GraphicsPipelineConfiguration& graphicsPipelineConfiguration);

private:
	vkr::Context m_context;
	std::unique_ptr<vkr::Instance> m_instance;
	DeviceDetails m_device;
	QueueDetails m_queues;
	SurfaceDetails m_surface;
	SwapchainDetails m_swapchain;
	GraphicsPipeline* m_pipeline;
	CommandPool* m_commandPool;
	GPUMemory* m_gpuMemory;

	std::unique_ptr<vkr::DebugUtilsMessengerEXT> m_debugMessenger;

	GraphicsPipelineConfiguration m_pipelineConfig;
	bool m_recreateSwapchain;
};

