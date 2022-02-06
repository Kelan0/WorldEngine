#include "GraphicsManager.h"
#include "../Application.h"
#include "GraphicsPipeline.h"
#include "CommandPool.h"


enum QueueType {
	QUEUE_TYPE_GRAPHICS_BIT = VK_QUEUE_GRAPHICS_BIT,
	QUEUE_TYPE_COMPUTE_BIT = VK_QUEUE_COMPUTE_BIT,
	QUEUE_TYPE_TRANSFER_BIT = VK_QUEUE_TRANSFER_BIT,
	QUEUE_TYPE_SPARSE_BINDING_BIT = VK_QUEUE_SPARSE_BINDING_BIT,
	QUEUE_TYPE_PROTECTED_BIT = VK_QUEUE_PROTECTED_BIT,
	QUEUE_TYPE_PRESENT_BIT = 0x800
};


#define VK_PFN(funcName) PFN_ ## funcName
#define VK_FUNC(funcName) ((VK_PFN(funcName))m_instance->getProcAddr(#funcName))


QueueDetails::QueueDetails() {}

GraphicsManager::GraphicsManager() {
	m_instance = NULL;
	m_device.device = NULL;
	m_device.physicalDevice = NULL;
	m_surface.surface = NULL;
	m_swapchain.swapchain = NULL;
	m_swapchain.maxFramesInFlight = 2;
	m_swapchain.currentFrameIndex = 0;
	m_pipeline = NULL;
	m_commandPool = NULL;
	m_debugMessenger = NULL;

}

GraphicsManager::~GraphicsManager() {
	printf("Uninitializing graphics engine\n");

	m_swapchain.commandBuffers.clear();
	m_swapchain.framebuffers.clear();
	m_swapchain.imageViews.clear();

	delete m_commandPool;
	delete m_pipeline;
}

bool GraphicsManager::init(SDL_Window* windowHandle, const char* applicationName) {
	printf("Initializing graphics engine\n");

	if (!createVulkanInstance(windowHandle, applicationName)) {
		return false;
	}

	if (!createSurface(windowHandle)) {
		return false;
	}

	if (!selectPhysicalDevice()) {
		return false;
	}

	std::vector<const char*> deviceLayers;
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::unordered_map<std::string, uint32_t> queueLayout;
	queueLayout.insert(std::make_pair("graphics_main", QUEUE_TYPE_GRAPHICS_BIT | QUEUE_TYPE_PRESENT_BIT));
	queueLayout.insert(std::make_pair("compute_main", QUEUE_TYPE_COMPUTE_BIT));
	queueLayout.insert(std::make_pair("transfer_main", QUEUE_TYPE_TRANSFER_BIT));

	if (!createLogicalDevice(deviceLayers, deviceExtensions, NULL, queueLayout)) {
		return false;
	}

	CommandPoolConfiguration commandPoolConfig;
	commandPoolConfig.device = m_device.device;
	commandPoolConfig.queueFamilyIndex = m_queues.graphicsQueueFamilyIndex.value();
	commandPoolConfig.resetCommandBuffer = true;
	commandPoolConfig.transient = false;
	m_commandPool = CommandPool::create(commandPoolConfig);

	if (!recreateSwapchain()) {
		return false;
	}


	return true;
}

bool GraphicsManager::createVulkanInstance(SDL_Window* windowHandle, const char* applicationName) {

	vk::ApplicationInfo appInfo;
	appInfo.setPApplicationName(applicationName);
	appInfo.setApplicationVersion(0);
	appInfo.setPEngineName("WorldEngine");
	appInfo.setEngineVersion(0);
	appInfo.setApiVersion(VK_API_VERSION_1_2);

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	if (enableValidationLayers) {
		printf("Enabling Vulkan validation layers\n");
	}

	uint32_t instanceExtensionCount;
	SDL_Vulkan_GetInstanceExtensions(windowHandle, &instanceExtensionCount, NULL);

	std::vector<const char*> instanceExtensions;
	instanceExtensions.resize(instanceExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(windowHandle, &instanceExtensionCount, instanceExtensions.data());

	if (enableValidationLayers) {
		instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	std::vector<const char*> layerNames;

	if (enableValidationLayers) {
		if (!selectValidationLayers(layerNames)) {
			return false;
		}
	}

	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.setPApplicationInfo(&appInfo);
	instanceInfo.setPEnabledLayerNames(layerNames);
	instanceInfo.setPEnabledExtensionNames(instanceExtensions);

	printf("Creating vulkan instance\n");
	m_instance = std::make_unique<vkr::Instance>(m_context, instanceInfo);

	if (enableValidationLayers) {
		if (!createDebugUtilsMessenger()) {
			return false;
		}
	}

	return true;
}

bool GraphicsManager::selectValidationLayers(std::vector<const char*>& layerNames) {

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation",
	};

	std::vector<vk::LayerProperties> availableLayerProperties = m_context.enumerateInstanceLayerProperties();

	for (const char* layerName : validationLayers) {
		bool layerFound = false;

		for (const auto& layer : availableLayerProperties) {
			if (strcmp(layerName, layer.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			printf("Required validation layer \"%s\" was not found\n", layerName);
			return false;
		}

		layerNames.push_back(layerName);
	}

	return true;
}

bool GraphicsManager::createDebugUtilsMessenger() {
	printf("Creating debug messenger\n");

	struct Validator {
		static VKAPI_ATTR VkBool32 VKAPI_CALL validate(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
			if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				printf("%s\n", pCallbackData->pMessage);
			}

			if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				// Abort erroneous method call
				return true;
			}

			return false;
		}
	};
	vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo;
	messengerCreateInfo.setMessageSeverity(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

	messengerCreateInfo.setMessageType(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);

	messengerCreateInfo.setPfnUserCallback(Validator::validate);

	m_debugMessenger = std::make_unique<vkr::DebugUtilsMessengerEXT>(*m_instance, messengerCreateInfo);
	
	return true;
}

bool GraphicsManager::createSurface(SDL_Window* windowHandle) {
	printf("Creating Vulkan SDL surface\n");

	VkSurfaceKHR surface;
	if (!SDL_Vulkan_CreateSurface(windowHandle, **m_instance, &surface)) {
		printf("Failed to create Vulkan surface for SDL window: %s\n", SDL_GetError());
		return false;
	}

	m_surface.surface = std::make_unique<vkr::SurfaceKHR>(*m_instance, surface);
	return true;
}

bool GraphicsManager::comparePhysicalDevices(const vkr::PhysicalDevice& physicalDevice1, const vkr::PhysicalDevice& physicalDevice2) const {
	vk::PhysicalDeviceProperties props1 = physicalDevice1.getProperties();
	vk::PhysicalDeviceProperties props2 = physicalDevice2.getProperties();

	// Prioritise discrete device
	if (props1.deviceType != props2.deviceType) {
		if (props1.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return true; // physicalDevice1 is better than physicalDevice2
		if (props2.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return false; // physicalDevice2 is better than physicalDevice1
	}

	// Both device types are the same

	vk::PhysicalDeviceFeatures features1 = physicalDevice1.getFeatures();
	vk::PhysicalDeviceFeatures features2 = physicalDevice2.getFeatures();

	constexpr int maxFeatureCount = sizeof(vk::PhysicalDeviceFeatures) / sizeof(VkBool32);

	// Prioritise device with more features
	int featureComparator = 0;
	for (int i = 0; i < maxFeatureCount; ++i) {
		if (reinterpret_cast<VkBool32*>(&features1)[i])
			--featureComparator; 
		if (reinterpret_cast<VkBool32*>(&features2)[i])
			++featureComparator;
	}

	if (featureComparator < 0)
		return true; // device1 supports more features
	if (featureComparator > 0)
		return false; // device2 supports more features

	// Prioritise device with more memory
	vk::PhysicalDeviceMemoryProperties mem1 = physicalDevice1.getMemoryProperties();
	vk::PhysicalDeviceMemoryProperties mem2 = physicalDevice2.getMemoryProperties();

	uint64_t vram1 = 0;
	uint64_t vram2 = 0;

	for (int i = 0; i < mem1.memoryHeapCount; ++i)
		if ((mem1.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal)) 
			vram1 += mem1.memoryHeaps[i].size;

	for (int i = 0; i < mem2.memoryHeapCount; ++i)
		if ((mem2.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal)) 
			vram2 += mem2.memoryHeaps[i].size;

	if (vram1 > vram2) return true; // device1 has more vram
	if (vram1 < vram2) return false; // device2 has more vram

	// Prioritise device with better limits
	if (props1.limits.maxImageDimension3D > props2.limits.maxImageDimension3D) return true; // device1 supports higher resolution 3D textures
	if (props1.limits.maxImageDimension3D < props2.limits.maxImageDimension3D) return false; // device2 supports higher resolution 3D textures
	if (props1.limits.maxImageDimension2D > props2.limits.maxImageDimension2D) return true; // device1 supports higher resolution 2D textures
	if (props1.limits.maxImageDimension2D < props2.limits.maxImageDimension2D) return false; // device2 supports higher resolution 2D textures
	if (props1.limits.maxImageDimension1D > props2.limits.maxImageDimension1D) return true; // device1 supports higher resolution 1D textures
	if (props1.limits.maxImageDimension1D < props2.limits.maxImageDimension1D) return false; // device2 supports higher resolution 1D textures
	if (props1.limits.maxImageDimensionCube > props2.limits.maxImageDimensionCube) return true; // device1 supports higher resolution cube textures
	if (props1.limits.maxImageDimensionCube < props2.limits.maxImageDimensionCube) return false; // device2 supports higher resolution cube textures
	if (props1.limits.maxMemoryAllocationCount > props2.limits.maxMemoryAllocationCount) return true; // device1 supports more allocated memory
	if (props1.limits.maxMemoryAllocationCount < props2.limits.maxMemoryAllocationCount) return false; // device2 supports more allocated memory
	if (props1.limits.maxImageArrayLayers > props2.limits.maxImageArrayLayers) return true; // device1 supports more image array layers
	if (props1.limits.maxImageArrayLayers < props2.limits.maxImageArrayLayers) return false; // device2 supports more image array layers
	if (props1.limits.maxTexelBufferElements > props2.limits.maxTexelBufferElements) return true; // device1 supports a larger texel buffer
	if (props1.limits.maxTexelBufferElements < props2.limits.maxTexelBufferElements) return false; // device2 supports a larger texel buffer
	if (props1.limits.maxUniformBufferRange > props2.limits.maxUniformBufferRange) return true; // device1 supports larger uniform buffers
	if (props1.limits.maxUniformBufferRange < props2.limits.maxUniformBufferRange) return false; // device2 supports larger uniform buffers
	if (props1.limits.maxStorageBufferRange > props2.limits.maxStorageBufferRange) return true; // device1 supports larger storage buffers
	if (props1.limits.maxStorageBufferRange < props2.limits.maxStorageBufferRange) return false; // device2 supports larger storage buffers
	// TODO: compare more properties

	return false; // Devices are the same...
}

bool GraphicsManager::isPhysicalDeviceSuitable(const vkr::PhysicalDevice& physicalDevice) const {
	vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();

	if (deviceProperties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu &&
		deviceProperties.deviceType != vk::PhysicalDeviceType::eIntegratedGpu &&
		deviceProperties.deviceType != vk::PhysicalDeviceType::eVirtualGpu) {
		
		printf("Device \"%s\" is not a supported type\n", deviceProperties.deviceName);
		return false;
	}

	return true;
}

bool GraphicsManager::selectQueueFamilies(const vkr::PhysicalDevice& physicalDevice, const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties, uint32_t requiredQueueFlags, QueueDetails& queueFamilyIndices) {

	bool requiresGraphics = (requiredQueueFlags & QUEUE_TYPE_GRAPHICS_BIT) != 0;
	bool requiresCompute = (requiredQueueFlags & QUEUE_TYPE_COMPUTE_BIT) != 0;
	bool requiresTransfer = (requiredQueueFlags & QUEUE_TYPE_TRANSFER_BIT) != 0;
	bool requiresSparseBinding = (requiredQueueFlags & QUEUE_TYPE_SPARSE_BINDING_BIT) != 0;
	bool requiresProtected = (requiredQueueFlags & QUEUE_TYPE_PROTECTED_BIT) != 0;
	bool requiresPresent = (requiredQueueFlags & QUEUE_TYPE_PRESENT_BIT) != 0;;

	for (int i = 0; i < queueFamilyProperties.size(); ++i) {
		bool supportsGraphics = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics);
		bool supportsCompute = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute);
		bool supportsTransfer = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer);
		bool supportsSparseBinding = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eSparseBinding);
		bool supportsProtected = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eProtected);
		bool supportsPresent = (bool)physicalDevice.getSurfaceSupportKHR(i, **m_surface.surface);

		if (!queueFamilyIndices.graphicsQueueFamilyIndex.has_value() && supportsGraphics)
			queueFamilyIndices.graphicsQueueFamilyIndex = i;
		if (!queueFamilyIndices.computeQueueFamilyIndex.has_value() && supportsCompute)
			queueFamilyIndices.computeQueueFamilyIndex = i;
		if (!queueFamilyIndices.transferQueueFamilyIndex.has_value() && supportsTransfer)
			queueFamilyIndices.transferQueueFamilyIndex = i;
		if (!queueFamilyIndices.sparseBindingQueueFamilyIndex.has_value() && supportsSparseBinding)
			queueFamilyIndices.sparseBindingQueueFamilyIndex = i;
		if (!queueFamilyIndices.protectedQueueFamilyIndex.has_value() && supportsProtected)
			queueFamilyIndices.protectedQueueFamilyIndex = i;
		if (!queueFamilyIndices.presentQueueFamilyIndex.has_value() && supportsPresent)
			queueFamilyIndices.presentQueueFamilyIndex = i;
	}

	std::vector<std::string> missingQueueTypes;

	if (requiresGraphics && !queueFamilyIndices.graphicsQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("GRAPHICS");
	if (requiresCompute && !queueFamilyIndices.computeQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("COMPUTE");
	if (requiresTransfer && !queueFamilyIndices.transferQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("TRANSFER");
	if (requiresSparseBinding && !queueFamilyIndices.sparseBindingQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("SPARSE_BINDING");
	if (requiresProtected && !queueFamilyIndices.protectedQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("PROTECTED");
	if (requiresPresent && !queueFamilyIndices.presentQueueFamilyIndex.has_value())
		missingQueueTypes.push_back("PRESENT");

	if (missingQueueTypes.size() > 0) {
		vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();

		std::string requiredQueueTypes = "";
		for (int i = 0; i < missingQueueTypes.size(); ++i) {
			requiredQueueTypes += missingQueueTypes[i];
			if (i < missingQueueTypes.size() - 1)
				requiredQueueTypes += ", ";
		}

		printf("Device \"%s\" does not support the required queue types: [%s]\n", deviceProperties.deviceName, requiredQueueTypes.c_str());
		return false;
	}

	return true;
}

bool GraphicsManager::selectPhysicalDevice() {

	std::vector<vkr::PhysicalDevice> physicalDevices = m_instance->enumeratePhysicalDevices();

	// Sort physical devices based on desirability
	std::sort(physicalDevices.begin(), physicalDevices.end(), [this](const vkr::PhysicalDevice& first, const vkr::PhysicalDevice& second) {
		return comparePhysicalDevices(first, second);
	});

	m_device.physicalDevice = NULL;
	std::vector<int> queueFamilyIndices;

	// Filter out physical devices missing required features
	for (int i = 0; i < physicalDevices.size(); ++i) {
		if (!isPhysicalDeviceSuitable(physicalDevices[i])) {
			continue;
		}

		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevices[i].getQueueFamilyProperties();

		QueueDetails queueFamilyIndices = {};
		uint32_t requiredQueueFlags = QUEUE_TYPE_GRAPHICS_BIT | QUEUE_TYPE_COMPUTE_BIT | QUEUE_TYPE_TRANSFER_BIT | QUEUE_TYPE_PRESENT_BIT;
		if (!selectQueueFamilies(physicalDevices[i], queueFamilyProperties, requiredQueueFlags, queueFamilyIndices)) {
			continue;
		}

		m_device.physicalDevice = std::make_unique<vkr::PhysicalDevice>(std::move(physicalDevices[i]));
		m_queues = queueFamilyIndices;

		break;
	}

	if (m_device.physicalDevice == NULL) {
		printf("No physical devices were suitable for rendering\n");
		return false;
	}

	printf("Graphics engine selected physical device \"%s\"\n", m_device.physicalDevice->getProperties().deviceName.data());

	return true;
}

bool GraphicsManager::createLogicalDevice(std::vector<const char*> const& enabledLayers, std::vector<const char*> const& enabledExtensions, vk::PhysicalDeviceFeatures* enabledFeatures, std::unordered_map<std::string, uint32_t> queueLayout) {
	printf("Creating logical device\n");

	std::set<uint32_t> uniqueQueueFamilyIndices;
	for (int i = 0; i < m_queues.indices.size(); ++i) {
		if (m_queues.indices[i].has_value())
			uniqueQueueFamilyIndices.insert(m_queues.indices[i].value());
	}

	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_device.physicalDevice->getQueueFamilyProperties();

	// TODO: not this... need a way to actually give queues priorities
	std::vector<float> queuePriorities = { 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };

	std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;

	std::unordered_map<uint32_t, std::vector<std::string>> queueIndexMap;

	for (auto it = uniqueQueueFamilyIndices.begin(); it != uniqueQueueFamilyIndices.end() && queueLayout.size() > 0; ++it) {
		uint32_t queueFamilyIndex = *it;

		std::vector<std::string>& queueIds = queueIndexMap[queueFamilyIndex];

		uint32_t queueFamilyFlags = (uint32_t)queueFamilyProperties[queueFamilyIndex].queueFlags; // Is this casts safe?

		if (m_queues.presentQueueFamilyIndex == queueFamilyIndex)
			queueFamilyFlags |= QUEUE_TYPE_PRESENT_BIT; // Add custom PRESENT bit if this queue family supports PRESENT

		for (auto it2 = queueLayout.begin(); it2 != queueLayout.end() && queueIds.size() < queueFamilyProperties[queueFamilyIndex].queueCount;) {
			const std::string& id = it2->first;
			uint32_t queueLayoutFlags = it2->second;


			if ((queueFamilyFlags & queueLayoutFlags) == queueLayoutFlags) {
				queueIds.push_back(id);
				it2 = queueLayout.erase(it2);
			} else {
				++it2;
			}
		}

		if (queueIds.size() == 0) {
			printf("Could not initialize the desired queue layout for queue family %d\n", queueFamilyIndex);
			continue;
		}

		vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
		deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		deviceQueueCreateInfo.queueCount = queueIds.size();
		deviceQueueCreateInfo.pQueuePriorities = queuePriorities.data();
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	if (queueLayout.size() > 0) {
		// Some required queues are left over...
		printf("Could not initialize logical device with all required queues\n");
		return false;
	}

	vk::DeviceCreateInfo createInfo;
	createInfo.setQueueCreateInfos(deviceQueueCreateInfos);
	createInfo.setPEnabledLayerNames(enabledLayers);
	createInfo.setPEnabledExtensionNames(enabledExtensions);
	createInfo.setPEnabledFeatures(enabledFeatures);
	m_device.device = std::make_shared<vkr::Device>(*m_device.physicalDevice, createInfo);

	for (auto it = queueIndexMap.begin(); it != queueIndexMap.end(); ++it) {
		uint32_t queueFamilyIndex = it->first;
		const std::vector<std::string>& ids = it->second;

		for (uint32_t queueIndex = 0; queueIndex < ids.size(); ++queueIndex) {
			m_queues.queues.insert(std::make_pair(ids[queueIndex], std::make_shared<vkr::Queue>(*m_device.device, queueFamilyIndex, queueIndex)));
		}
	}

	return true;
}

bool GraphicsManager::initSurfaceDetails() {
	VkResult result;

	m_surface.capabilities = m_device.physicalDevice->getSurfaceCapabilitiesKHR(**m_surface.surface);

	std::vector<vk::SurfaceFormatKHR> formats = m_device.physicalDevice->getSurfaceFormatsKHR(**m_surface.surface);
	
	if (formats.size() == 0) {
		printf("Device \"%s\" supports no surface formats\n", m_device.physicalDevice->getProperties().deviceName.data());
		return false;
	}

	auto selectedFormat = formats.end();
	for (auto it = formats.begin(); it != formats.end() && selectedFormat == formats.end(); ++it) {
		if (it->format == vk::Format::eB8G8R8A8Srgb && it->colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			selectedFormat = it;
	}

	if (selectedFormat != formats.end()) {
		m_surface.surfaceFormat = *selectedFormat;
	} else {
		printf("Preferred surface format and colour space was not found. Defaulting to first available option\n");
		m_surface.surfaceFormat = formats[0];
	}

	std::vector<vk::PresentModeKHR> presentModes = m_device.physicalDevice->getSurfacePresentModesKHR(**m_surface.surface);
	
	if (presentModes.size() == 0) {
		printf("Device \"%s\" supports no present modes\n", m_device.physicalDevice->getProperties().deviceName.data());
		return false;
	}

	auto presentMode = std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox);
	if (presentMode != presentModes.end()) {
		m_surface.presentMode = *presentMode;
	} else {
		printf("Preferred surface present mode was not found. Defaulting to first available option\n");
		m_surface.presentMode = presentModes[0];
	}

	return true;
}

bool GraphicsManager::recreateSwapchain() {
	m_device.device->waitIdle();

	m_swapchain.commandBuffers.clear();
	for (int i = 0; i < m_swapchain.maxFramesInFlight; ++i)
		if (m_commandPool->hasCommandBuffer("swapchain_cmd" + std::to_string(i)))
			m_commandPool->freeCommandBuffer("swapchain_cmd" + std::to_string(i));

	m_swapchain.framebuffers.clear();
	m_swapchain.imageViews.clear();
	delete m_pipeline;
	m_swapchain.swapchain.reset();

	initSurfaceDetails();

	glm::ivec2 windowSize = Application::instance()->getWindowSize();
	m_swapchain.imageExtent = vk::Extent2D((uint32_t)windowSize.x, (uint32_t)windowSize.y);
	m_swapchain.imageExtent.width = glm::clamp(m_swapchain.imageExtent.width, m_surface.capabilities.minImageExtent.width, m_surface.capabilities.maxImageExtent.width);
	m_swapchain.imageExtent.height = glm::clamp(m_swapchain.imageExtent.height, m_surface.capabilities.minImageExtent.height, m_surface.capabilities.maxImageExtent.height);

	uint32_t imageCount = m_surface.capabilities.minImageCount + 1;
	if (m_surface.capabilities.maxImageCount > 0) {
		imageCount = glm::min(imageCount, m_surface.capabilities.maxImageCount);
	}

	m_swapchain.maxFramesInFlight = 2;

	vk::SwapchainCreateInfoKHR createInfo;
	createInfo.setSurface(**m_surface.surface);
	createInfo.setMinImageCount(imageCount);
	createInfo.setImageFormat(m_surface.surfaceFormat.format);
	createInfo.setImageColorSpace(m_surface.surfaceFormat.colorSpace);
	createInfo.setImageExtent(m_swapchain.imageExtent);
	createInfo.setImageArrayLayers(1);
	createInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

	std::vector<uint32_t> queueFamilyIndices = { m_queues.graphicsQueueFamilyIndex.value(), m_queues.presentQueueFamilyIndex.value() };

	if (m_queues.graphicsQueueFamilyIndex != m_queues.presentQueueFamilyIndex) {
		createInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
		createInfo.setQueueFamilyIndices(queueFamilyIndices);
	} else {
		createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
		createInfo.setQueueFamilyIndexCount(0);
	}

	createInfo.setPreTransform(m_surface.capabilities.currentTransform);
	createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
	createInfo.setPresentMode(m_surface.presentMode);
	createInfo.setClipped(true);
	//createInfo.setOldSwapchain(NULL);

	m_swapchain.swapchain = std::make_unique<vkr::SwapchainKHR>(*m_device.device, createInfo);
	
	if (!createSwapchainImages()) {
		return false;
	}

	GraphicsPipelineConfiguration pipelineConfig;
	pipelineConfig.device = m_device.device;
	pipelineConfig.width = 0;
	pipelineConfig.height = 0;
	pipelineConfig.vertexShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.vert";
	pipelineConfig.fragmentShader = "D:/Code/ActiveProjects/WorldEngine/res/shaders/main.frag";
	m_pipeline = GraphicsPipeline::create(pipelineConfig);
	if (m_pipeline == NULL) {
		return false;
	}

	if (!createSwapchainFramebuffers()) {
		return false;
	}

	m_swapchain.commandBuffers.resize(m_swapchain.maxFramesInFlight);
	for (int i = 0; i < m_swapchain.maxFramesInFlight; ++i) {
		std::shared_ptr<vkr::CommandBuffer> commandBuffer = m_commandPool->allocateCommandBuffer("swapchain_cmd" + std::to_string(i), { vk::CommandBufferLevel::ePrimary });
		m_swapchain.commandBuffers[i] = std::move(commandBuffer);
	}

	m_swapchain.imageAvailableSemaphores.clear();
	m_swapchain.imageAvailableSemaphores.resize(m_swapchain.maxFramesInFlight);

	m_swapchain.renderFinishedSemaphores.clear();
	m_swapchain.renderFinishedSemaphores.resize(m_swapchain.maxFramesInFlight);

	m_swapchain.inFlightFences.clear();
	m_swapchain.inFlightFences.resize(m_swapchain.maxFramesInFlight);

	vk::SemaphoreCreateInfo semaphoreCreateInfo;
	vk::FenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

	for (int i = 0; i < m_swapchain.maxFramesInFlight; ++i) {
		m_swapchain.imageAvailableSemaphores[i] = std::make_unique<vkr::Semaphore>(*m_device.device, semaphoreCreateInfo);

		m_swapchain.renderFinishedSemaphores[i] = std::make_unique<vkr::Semaphore>(*m_device.device, semaphoreCreateInfo);

		m_swapchain.inFlightFences[i] = std::make_unique<vkr::Fence>(*m_device.device, fenceCreateInfo);
	}

	m_swapchain.currentFrameIndex = 0;

	return true;
}

bool GraphicsManager::createSwapchainImages() {
	std::vector<VkImage> images = m_swapchain.swapchain->getImages();
	m_swapchain.imageViews.resize(images.size());
	
	for (int i = 0; i < images.size(); ++i) {
		vk::ImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.setImage(images[i]);
		imageViewCreateInfo.setViewType(vk::ImageViewType::e2D);
		imageViewCreateInfo.setFormat(m_surface.surfaceFormat.format);
		imageViewCreateInfo.components.setR(vk::ComponentSwizzle::eIdentity);
		imageViewCreateInfo.components.setG(vk::ComponentSwizzle::eIdentity);
		imageViewCreateInfo.components.setB(vk::ComponentSwizzle::eIdentity);
		imageViewCreateInfo.components.setA(vk::ComponentSwizzle::eIdentity);
		imageViewCreateInfo.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
		imageViewCreateInfo.subresourceRange.setBaseMipLevel(0);
		imageViewCreateInfo.subresourceRange.setLevelCount(1);
		imageViewCreateInfo.subresourceRange.setBaseArrayLayer(0);
		imageViewCreateInfo.subresourceRange.setLayerCount(1);

		m_swapchain.imageViews[i] = std::make_shared<vkr::ImageView>(*m_device.device, imageViewCreateInfo);
	}

	m_swapchain.imagesInFlight.clear();
	m_swapchain.imagesInFlight.resize(images.size(), VK_NULL_HANDLE);

	return true;
}

bool GraphicsManager::createSwapchainFramebuffers() {
	m_swapchain.framebuffers.resize(m_swapchain.imageViews.size());

	for (int i = 0; i < m_swapchain.imageViews.size(); ++i) {
		vk::ImageView colourAttachment = **m_swapchain.imageViews[i];
		vk::FramebufferCreateInfo framebufferCreateInfo;
		framebufferCreateInfo.setRenderPass(*m_pipeline->getRenderPass());
		framebufferCreateInfo.setPAttachments(&colourAttachment);
		framebufferCreateInfo.setAttachmentCount(1);
		framebufferCreateInfo.setWidth(m_swapchain.imageExtent.width);
		framebufferCreateInfo.setHeight(m_swapchain.imageExtent.height);
		framebufferCreateInfo.setLayers(1);

		m_swapchain.framebuffers[i] = std::make_shared<vkr::Framebuffer>(*m_device.device, framebufferCreateInfo);
	}
	return true;
}

bool GraphicsManager::beginFrame(vk::CommandBuffer& outCommandBuffer, vk::Framebuffer& outFramebuffer) {

	const vk::Device& device = **m_device.device;
	const vk::Semaphore& imageAvailableSemaphore = **m_swapchain.imageAvailableSemaphores[m_swapchain.currentFrameIndex];
	const vk::Fence& frameFence = **m_swapchain.inFlightFences[m_swapchain.currentFrameIndex];
	const vk::SwapchainKHR& swapchain = **m_swapchain.swapchain;

	m_device.device->waitForFences({ frameFence }, true, UINT64_MAX);

	if (m_swapchain.imageExtent.width != Application::instance()->getWindowSize().x ||
		m_swapchain.imageExtent.height != Application::instance()->getWindowSize().y) {
		bool recreated = recreateSwapchain();
#if _DEBUG
		if (!recreated) {
			printf("Failed to recreate swapchain\n");
			assert(false);
		}
#endif
		return false;
	}

	VkResult result;
	result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &m_swapchain.currentImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		bool recreated = recreateSwapchain();
#if _DEBUG
		if (!recreated) {
			printf("Failed to recreate swapchain\n");
			assert(false);
		}
#endif
	} 
	
	if (result != VK_SUCCESS) {
		return false;
	}

	vk::CommandBuffer commandBuffer = getCurrentCommandBuffer();
	vk::Framebuffer framebuffer = getCurrentFramebuffer();

	if (!commandBuffer || !framebuffer) {
		return false;
	}


	vk::CommandBufferBeginInfo beginInfo;
	commandBuffer.begin(beginInfo);

	outCommandBuffer = commandBuffer;
	outFramebuffer = framebuffer;

	return true;
}

void GraphicsManager::endFrame() {
	const vk::Device& device = **m_device.device;
	const vk::Semaphore& imageAvailableSemaphore = **m_swapchain.imageAvailableSemaphores[m_swapchain.currentFrameIndex];
	const vk::Semaphore& renderFinishedSemaphore = **m_swapchain.renderFinishedSemaphores[m_swapchain.currentFrameIndex];
	const vk::Fence& frameFence = **m_swapchain.inFlightFences[m_swapchain.currentFrameIndex];
	const vk::SwapchainKHR& swapchain = **m_swapchain.swapchain;
	const vk::CommandBuffer& commandBuffer = getCurrentCommandBuffer();

	commandBuffer.end();

	if (m_swapchain.imagesInFlight[m_swapchain.currentImageIndex]) {
		device.waitForFences({ m_swapchain.imagesInFlight[m_swapchain.currentImageIndex] }, true, UINT64_MAX);
	}
	
	m_swapchain.imagesInFlight[m_swapchain.currentImageIndex] = frameFence;

	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	std::vector <vk::SubmitInfo> submitInfos;
	vk::SubmitInfo& submitInfo = submitInfos.emplace_back();
	submitInfo.setWaitSemaphoreCount(1);
	submitInfo.setPWaitSemaphores(&imageAvailableSemaphore);
	submitInfo.setPWaitDstStageMask(waitStages);
	submitInfo.setCommandBufferCount(1);
	submitInfo.setPCommandBuffers(&commandBuffer);
	submitInfo.setSignalSemaphoreCount(1);
	submitInfo.setPSignalSemaphores(&renderFinishedSemaphore);

	const vk::Queue& queue = **m_queues.queues["graphics_main"];

	m_device.device->resetFences({ frameFence });

	// Submit the queue, signal frameFence once the commands are executed
	queue.submit(submitInfos, frameFence);

	vk::PresentInfoKHR presentInfo;
	presentInfo.setWaitSemaphoreCount(1);
	presentInfo.setPWaitSemaphores(&renderFinishedSemaphore);
	presentInfo.setSwapchainCount(1);
	presentInfo.setPSwapchains(&swapchain);
	presentInfo.setPImageIndices(&m_swapchain.currentImageIndex);

	VkResult result = vkQueuePresentKHR(queue, reinterpret_cast<const VkPresentInfoKHR*>(&presentInfo));

	// Rather frustratingly, vk::raii::Queue::presentKHR throws an exception on vk::Result::eErrorOutOfDateKHR, but we need that result to recreate the swapchain.
	// vk::Result result = queue.presentKHR(presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		bool recreated = recreateSwapchain();
#if _DEBUG
		if (!recreated) {
			printf("Failed to recreate swapchain\n");
			assert(false);
		}
#endif
	}

	m_swapchain.currentFrameIndex = (m_swapchain.currentFrameIndex + 1) % m_swapchain.maxFramesInFlight;
}

std::shared_ptr<vkr::Device> GraphicsManager::getDevice() const {
	return m_device.device;
}

const vk::CommandBuffer& GraphicsManager::getCurrentCommandBuffer() const {
	return **m_swapchain.commandBuffers[m_swapchain.currentFrameIndex];
}

const vk::Framebuffer& GraphicsManager::getCurrentFramebuffer() const {
	return **m_swapchain.framebuffers[m_swapchain.currentImageIndex];
}

GraphicsPipeline& GraphicsManager::pipeline() {
	return *m_pipeline;
}

CommandPool& GraphicsManager::commandPool() {
	return *m_commandPool;
}

glm::ivec2 GraphicsManager::getResolution() const {
	return glm::ivec2(m_swapchain.imageExtent.width, m_swapchain.imageExtent.height);
}

const vk::Extent2D& GraphicsManager::getImageExtent() const {
	return m_swapchain.imageExtent;
}

vk::Format GraphicsManager::getColourFormat() const {
	return m_surface.surfaceFormat.format;
}

vk::ColorSpaceKHR GraphicsManager::getColourSpace() const {
	return m_surface.surfaceFormat.colorSpace;
}