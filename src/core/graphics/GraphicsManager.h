#pragma once

#include "../core.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include <SDL.h>
#include <SDL_vulkan.h>

namespace vkr = vk::raii;

class GraphicsPipeline;
class CommandPool;

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
};

struct SurfaceDetails {
	std::unique_ptr<vkr::SurfaceKHR> surface;
	vk::SurfaceCapabilitiesKHR capabilities;
	vk::SurfaceFormatKHR surfaceFormat;
	vk::PresentModeKHR presentMode;
};

struct SwapchainDetails {
	std::unique_ptr<vkr::SwapchainKHR> swapchain;
	std::vector<std::shared_ptr<vkr::ImageView>> imageViews;
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
	NO_COPY(GraphicsManager);

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

	bool beginFrame(vk::CommandBuffer& commandBuffer, vk::Framebuffer& framebuffer);

	void endFrame();

	std::shared_ptr<vkr::Device> getDevice() const;

	const vk::CommandBuffer& getCurrentCommandBuffer() const;

	const vk::Framebuffer& getCurrentFramebuffer() const;

	GraphicsPipeline& pipeline();

	CommandPool& commandPool();

	glm::ivec2 getResolution() const;

	const vk::Extent2D& getImageExtent() const;

	vk::Format getColourFormat() const;

	vk::ColorSpaceKHR getColourSpace() const;


private:
	vkr::Context m_context;
	std::unique_ptr<vkr::Instance> m_instance;
	DeviceDetails m_device;
	QueueDetails m_queues;
	SurfaceDetails m_surface;
	SwapchainDetails m_swapchain;
	GraphicsPipeline* m_pipeline;
	CommandPool* m_commandPool;

	std::unique_ptr<vkr::DebugUtilsMessengerEXT> m_debugMessenger;

};

