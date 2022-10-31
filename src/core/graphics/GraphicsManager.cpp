#include "core/graphics/GraphicsManager.h"
#include "core/graphics/RenderPass.h"
#include "core/graphics/CommandPool.h"
#include "core/graphics/DeviceMemory.h"
#include "core/graphics/DescriptorSet.h"
#include "core/graphics/ImageView.h"
#include "core/graphics/Image2D.h"
#include "core/graphics/Buffer.h"
#include "core/graphics/Framebuffer.h"
#include "core/application/Engine.h"
#include "core/application/Application.h"
#include "core/engine/event/EventDispatcher.h"
#include "core/engine/event/GraphicsEvents.h"
#include "core/util/Profiler.h"
#include "core/util/Util.h"

uint64_t GraphicsManager::s_nextResourceID = 0;


#define VK_PFN(funcName) PFN_ ## funcName
#define VK_FUNC(funcName) ((VK_PFN(funcName))m_instance->getProcAddr(#funcName))


QueueDetails::QueueDetails() {};

GraphicsManager::GraphicsManager():
        m_instance(nullptr),
        m_renderPass(nullptr),
        m_commandPool(nullptr),
        m_descriptorPool(nullptr),
        m_memory(nullptr),
        m_debugMessenger(nullptr),
        m_preferredPresentMode(vk::PresentModeKHR::eMailbox),
        m_isInitialized(false),
        m_recreateSwapchain(false),
        m_resolutionChanged(false),
        m_directImagePresentEnabled(false),
        m_swapchainImageSampled(false),
        m_swapchainBufferMode(SwapchainBufferMode_TripleBuffer) {
    m_swapchain.currentFrameIndex = 0;
    m_swapchain.currentImageIndex = UINT32_MAX;
}

GraphicsManager::~GraphicsManager() {
    printf("Uninitializing graphics engine\n");

    //DescriptorSetLayout::clearCache();
    //Buffer::resetStagingBuffer();

//    m_swapchain.depthImageView.reset();
//    m_swapchain.depthImage.reset();
    m_swapchain.commandBuffers.clear();
    m_swapchain.framebuffers.clear();
    m_swapchain.imageViews.clear();

    if (m_renderPass.use_count() > 1)
        printf("Destroyed GraphicsManager but RenderPass has %llu external references\n", (uint64_t)m_renderPass.use_count() - 1);
    m_renderPass.reset();

    if (m_descriptorPool.use_count() > 1)
        printf("Destroyed GraphicsManager but DescriptorPool has %llu external references\n", (uint64_t)m_descriptorPool.use_count() - 1);

    if (m_commandPool.use_count() > 1)
        printf("Destroyed GraphicsManager but CommandPool has %llu external references\n", (uint64_t)m_commandPool.use_count() - 1);

    delete m_memory;
    m_descriptorPool.reset();
    m_commandPool.reset();
    //delete m_pipeline;

    if (m_device.device.use_count() > 1) {
        std::vector<std::string> ownerNames;
        m_device.device.getAllReferenceOwnerNames(ownerNames);
        printf("Destroyed GraphicsManager but the logical device still has %llu external references: [%s]\n", (uint64_t)m_device.device.use_count() - 1, Util::vector_to_string(ownerNames).c_str());
    }
}

bool GraphicsManager::init(SDL_Window* windowHandle, const char* applicationName) {
    PROFILE_SCOPE("GraphicsManager::init")

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
    std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
            VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME,
//            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    };

    std::unordered_map<std::string, uint32_t> queueLayout;
    queueLayout.insert(std::make_pair(QUEUE_GRAPHICS_MAIN, QueueType_GraphicsBit | QueueType_PresentBit));
    queueLayout.insert(std::make_pair(QUEUE_COMPUTE_MAIN, QueueType_ComputeBit));
    queueLayout.insert(std::make_pair(QUEUE_TRANSFER_MAIN, QueueType_TransferBit));
    queueLayout.insert(std::make_pair(QUEUE_GRAPHICS_TRANSFER_MAIN, QueueType_GraphicsBit | QueueType_TransferBit));

    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.fillModeNonSolid = true;
    deviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
    deviceFeatures.shaderUniformBufferArrayDynamicIndexing = true;
    deviceFeatures.shaderStorageImageArrayDynamicIndexing = true;
    deviceFeatures.shaderStorageBufferArrayDynamicIndexing = true;


    vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures;
    descriptorIndexingFeatures.shaderInputAttachmentArrayDynamicIndexing = true;
    descriptorIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing = true;
    descriptorIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing = true;
    descriptorIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderInputAttachmentArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing = true;
    descriptorIndexingFeatures.runtimeDescriptorArray = true;

    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
    extendedDynamicStateFeatures.extendedDynamicState = true;
    descriptorIndexingFeatures.setPNext(&extendedDynamicStateFeatures);

    if (!createLogicalDevice(deviceLayers, deviceExtensions, &deviceFeatures, &descriptorIndexingFeatures, queueLayout)) {
        return false;
    }

    if (!initSurfaceDetails()) {
        return false;
    }

    PROFILE_REGION("Initialize graphics resources")

    if (!createRenderPass()) {
        return false;
    }

    m_memory = new DeviceMemoryManager(m_device.device);
    //GPUMemoryConfiguration memoryConfig;
    //memoryConfig.device = m_device.device;
    //memoryConfig.size = (size_t)(6.0 * 1024 * 1024 * 1024); // Allocate 8 GiB
    ////memoryConfig.memoryPropertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
    //memoryConfig.memoryTypeBits = 0;
    //for (int i = 0; i < m_device.memoryProperties.memoryTypeCount; ++i)
    //	memoryConfig.memoryTypeBits |= (1 << i); // Enable all memory types
    //
    //m_gpuMemory = GPUMemory::create(memoryConfig);
    //if (m_gpuMemory == nullptr) {
    //	return false;
    //}

    CommandPoolConfiguration commandPoolConfig{};
    commandPoolConfig.device = m_device.device;
    commandPoolConfig.queueFamilyIndex = m_queues.queueFamilies.graphicsQueueFamilyIndex.value();
    commandPoolConfig.resetCommandBuffer = true;
    commandPoolConfig.transient = false;
    m_commandPool = SharedResource<CommandPool>(CommandPool::create(commandPoolConfig, "GraphicsManager-DefaultCommandPool"), "GraphicsManager-DefaultCommandPool");

    DescriptorPoolConfiguration descriptorPoolConfig{};
    descriptorPoolConfig.device = m_device.device;
    //descriptorPoolConfig.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    m_descriptorPool = SharedResource<DescriptorPool>(DescriptorPool::create(descriptorPoolConfig, "GraphicsManager-DefaultDescriptorPool"), "GraphicsManager-DefaultDescriptorPool");

    m_commandPool->allocateCommandBuffer("transfer_buffer", {vk::CommandBufferLevel::ePrimary});

    m_isInitialized = true;
    m_recreateSwapchain = true;
    return true;
}

bool GraphicsManager::createVulkanInstance(SDL_Window* windowHandle, const char* applicationName) {
    PROFILE_SCOPE("GraphicsManager::createVulkanInstance")

#ifdef USE_VOLK
    {
        VkResult result = volkInitialize();
        if (result != VK_SUCCESS) {
            printf("Failed to initialize VOLK: %s\n", vk::to_string((vk::Result)result).c_str());
            return false;
        }
    }
#endif

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
    SDL_Vulkan_GetInstanceExtensions(windowHandle, &instanceExtensionCount, nullptr);

    std::vector<const char*> instanceExtensions;
    instanceExtensions.resize(instanceExtensionCount);
    SDL_Vulkan_GetInstanceExtensions(windowHandle, &instanceExtensionCount, instanceExtensions.data());

    if (enableValidationLayers) {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

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

#ifdef USE_VOLK
    volkLoadInstance(**m_instance);
#endif

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

//    for (const vk::LayerProperties& layer : availableLayerProperties) {
//        printf("%s\n", layer.layerName);
//    }

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
                if (Engine::graphics()->doAbortOnVulkanError()) {
                    throw std::runtime_error(pCallbackData->pMessage);
                    return true; // Abort erroneous method call, Method will return eErrorValidationFailed and throw an exception
                }
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
    PROFILE_SCOPE("GraphicsManager::createSurface")
    printf("Creating Vulkan SDL surface\n");

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(windowHandle, (VkInstance)**m_instance, &surface)) {
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

    for (size_t i = 0; i < mem1.memoryHeapCount; ++i)
        if ((mem1.memoryHeaps[i].flags & vk::MemoryHeapFlagBits::eDeviceLocal))
            vram1 += mem1.memoryHeaps[i].size;

    for (size_t i = 0; i < mem2.memoryHeapCount; ++i)
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

        printf("Device \"%s\" is not a supported type\n", deviceProperties.deviceName.data());
        return false;
    }

    return true;
}

bool GraphicsManager::selectQueueFamilies(const vkr::PhysicalDevice& physicalDevice, const std::vector<vk::QueueFamilyProperties>& queueFamilyProperties, uint32_t requiredQueueFlags, QueueDetails& queueFamilyIndices) {
    bool requiresGraphics = (requiredQueueFlags & QueueType_GraphicsBit) != 0;
    bool requiresCompute = (requiredQueueFlags & QueueType_ComputeBit) != 0;
    bool requiresTransfer = (requiredQueueFlags & QueueType_TransferBit) != 0;
    bool requiresSparseBinding = (requiredQueueFlags & QueueType_SparseBindingBit) != 0;
    bool requiresProtected = (requiredQueueFlags & QueueType_ProtectedBit) != 0;
    bool requiresPresent = (requiredQueueFlags & QueueType_PresentBit) != 0;;

    for (int i = 0; i < queueFamilyProperties.size(); ++i) {
        bool supportsGraphics = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics);
        bool supportsCompute = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute);
        bool supportsTransfer = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer);
        bool supportsSparseBinding = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eSparseBinding);
        bool supportsProtected = (bool)(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eProtected);
        bool supportsPresent = (bool)physicalDevice.getSurfaceSupportKHR(i, **m_surface.surface);

        if (!queueFamilyIndices.queueFamilies.graphicsQueueFamilyIndex.has_value() && supportsGraphics)
            queueFamilyIndices.queueFamilies.graphicsQueueFamilyIndex = i;
        if (!queueFamilyIndices.queueFamilies.computeQueueFamilyIndex.has_value() && supportsCompute)
            queueFamilyIndices.queueFamilies.computeQueueFamilyIndex = i;
        if (!queueFamilyIndices.queueFamilies.transferQueueFamilyIndex.has_value() && supportsTransfer)
            queueFamilyIndices.queueFamilies.transferQueueFamilyIndex = i;
        if (!queueFamilyIndices.queueFamilies.sparseBindingQueueFamilyIndex.has_value() && supportsSparseBinding)
            queueFamilyIndices.queueFamilies.sparseBindingQueueFamilyIndex = i;
        if (!queueFamilyIndices.queueFamilies.protectedQueueFamilyIndex.has_value() && supportsProtected)
            queueFamilyIndices.queueFamilies.protectedQueueFamilyIndex = i;
        if (!queueFamilyIndices.queueFamilies.presentQueueFamilyIndex.has_value() && supportsPresent)
            queueFamilyIndices.queueFamilies.presentQueueFamilyIndex = i;
    }

    std::vector<std::string> missingQueueTypes;

    if (requiresGraphics && !queueFamilyIndices.queueFamilies.graphicsQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("GRAPHICS");
    if (requiresCompute && !queueFamilyIndices.queueFamilies.computeQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("COMPUTE");
    if (requiresTransfer && !queueFamilyIndices.queueFamilies.transferQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("TRANSFER");
    if (requiresSparseBinding && !queueFamilyIndices.queueFamilies.sparseBindingQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("SPARSE_BINDING");
    if (requiresProtected && !queueFamilyIndices.queueFamilies.protectedQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("PROTECTED");
    if (requiresPresent && !queueFamilyIndices.queueFamilies.presentQueueFamilyIndex.has_value())
        missingQueueTypes.push_back("PRESENT");

    if (missingQueueTypes.size() > 0) {
        vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();

        std::string requiredQueueTypes = "";
        for (int i = 0; i < missingQueueTypes.size(); ++i) {
            requiredQueueTypes += missingQueueTypes[i];
            if (i < missingQueueTypes.size() - 1)
                requiredQueueTypes += ", ";
        }

        printf("Device \"%s\" does not support the required queue types: [%s]\n", deviceProperties.deviceName.data(), requiredQueueTypes.c_str());
        return false;
    }

    return true;
}

bool GraphicsManager::selectPhysicalDevice() {
    PROFILE_SCOPE("GraphicsManager::selectPhysicalDevice")

    std::vector<vkr::PhysicalDevice> physicalDevices = m_instance->enumeratePhysicalDevices();

    // Sort physical devices based on desirability
    std::sort(physicalDevices.begin(), physicalDevices.end(), [this](const vkr::PhysicalDevice& first, const vkr::PhysicalDevice& second) {
        return comparePhysicalDevices(first, second);
    });

    m_device.physicalDevice = nullptr;

    // Filter out physical devices missing required features
    for (auto& physicalDevice : physicalDevices) {
        if (!isPhysicalDeviceSuitable(physicalDevice)) {
            continue;
        }

        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        QueueDetails queueFamilyIndices = {};
        uint32_t requiredQueueFlags = QueueType_GraphicsBit | QueueType_ComputeBit | QueueType_TransferBit | QueueType_PresentBit;
        if (!selectQueueFamilies(physicalDevice, queueFamilyProperties, requiredQueueFlags, queueFamilyIndices)) {
            continue;
        }

        m_device.physicalDevice = std::make_unique<vkr::PhysicalDevice>(std::move(physicalDevice));
        m_queues = queueFamilyIndices;

        break;
    }

    if (m_device.physicalDevice == nullptr) {
        printf("No physical devices were suitable for rendering\n");
        return false;
    }

    m_device.memoryProperties = m_device.physicalDevice->getMemoryProperties();
    m_device.physicalDeviceProperties = m_device.physicalDevice->getProperties();

    printf("Graphics engine selected physical device \"%s\"\n", m_device.physicalDevice->getProperties().deviceName.data());

    return true;
}

bool GraphicsManager::createLogicalDevice(std::vector<const char*> const& enabledLayers, std::vector<const char*> const& enabledExtensions, vk::PhysicalDeviceFeatures* enabledFeatures, void* pNext, std::unordered_map<std::string, uint32_t> queueLayout) {
    PROFILE_SCOPE("GraphicsManager::createLogicalDevice")

    printf("Creating logical device\n");

    std::set<uint32_t> uniqueQueueFamilyIndices;
    for (auto& index : m_queues.indices) {
        if (index.has_value())
            uniqueQueueFamilyIndices.insert(index.value());
    }

    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_device.physicalDevice->getQueueFamilyProperties();

    // TODO: not this... need a way to actually give queues priorities
    std::vector<float> queuePriorities = {0.0F, 0.0F, 0.0F, 0.0F, 0.0F};

    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;

    std::unordered_map<uint32_t, std::vector<std::string>> queueIndexMap;

    for (auto it = uniqueQueueFamilyIndices.begin(); it != uniqueQueueFamilyIndices.end() && !queueLayout.empty(); ++it) {
        uint32_t queueFamilyIndex = *it;

        std::vector<std::string>& queueIds = queueIndexMap[queueFamilyIndex];

        uint32_t queueFamilyFlags = (uint32_t)queueFamilyProperties[queueFamilyIndex].queueFlags; // Is this casts safe?

        if (m_queues.queueFamilies.presentQueueFamilyIndex == queueFamilyIndex)
            queueFamilyFlags |= QueueType_PresentBit; // Add custom PRESENT bit if this queue family supports PRESENT

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

        if (queueIds.empty()) {
            printf("Could not initialize the desired queue layout for queue family %d\n", queueFamilyIndex);
            continue;
        }

        vk::DeviceQueueCreateInfo deviceQueueCreateInfo;
        deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        deviceQueueCreateInfo.queueCount = (uint32_t)queueIds.size();
        deviceQueueCreateInfo.pQueuePriorities = queuePriorities.data();
        deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
    }

    if (!queueLayout.empty()) {
        // Some required queues are left over...
        printf("Could not initialize logical device with all required queues\n");
        return false;
    }

    vk::DeviceCreateInfo createInfo;
    createInfo.setPNext(pNext);
    createInfo.setQueueCreateInfos(deviceQueueCreateInfos);
    createInfo.setPEnabledLayerNames(enabledLayers);
    createInfo.setPEnabledExtensionNames(enabledExtensions);
    createInfo.setPEnabledFeatures(enabledFeatures);
    m_device.device = SharedResource<vkr::Device>(new vkr::Device(*m_device.physicalDevice, createInfo), "GraphicsManager-Device");

    for (auto& entry : queueIndexMap) {
        uint32_t queueFamilyIndex = entry.first;
        const std::vector<std::string>& ids = entry.second;

        for (uint32_t queueIndex = 0; queueIndex < ids.size(); ++queueIndex) {
            m_queues.queues.insert(std::make_pair(ids[queueIndex], std::make_shared<vkr::Queue>(*m_device.device, queueFamilyIndex, queueIndex)));
        }
    }

    loadVulkanDeviceExtensions(**m_device.device);

    return true;
}

bool GraphicsManager::initSurfaceDetails() {
    PROFILE_SCOPE("GraphicsManager::initSurfaceDetails")

    m_surface.capabilities = m_device.physicalDevice->getSurfaceCapabilitiesKHR(**m_surface.surface);

    glm::uvec2 windowSize = Application::instance()->getWindowSize();
    m_swapchain.imageExtent.width = glm::clamp(windowSize.x, m_surface.capabilities.minImageExtent.width, m_surface.capabilities.maxImageExtent.width);
    m_swapchain.imageExtent.height = glm::clamp(windowSize.y, m_surface.capabilities.minImageExtent.height, m_surface.capabilities.maxImageExtent.height);

    std::vector<vk::SurfaceFormatKHR> formats = m_device.physicalDevice->getSurfaceFormatsKHR(**m_surface.surface);

    if (formats.empty()) {
        printf("Device \"%s\" supports no surface formats\n", m_device.physicalDevice->getProperties().deviceName.data());
        return false;
    }

    bool useSRGB = false;

    auto selectedFormat = formats.end();
    for (auto it = formats.begin(); it != formats.end() && selectedFormat == formats.end(); ++it) {
        if (useSRGB) {
            if (it->format == vk::Format::eB8G8R8A8Srgb && it->colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                selectedFormat = it;
        } else {
            if (it->format == vk::Format::eB8G8R8A8Unorm && it->colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                selectedFormat = it;
        }
    }

    if (selectedFormat != formats.end()) {
        m_surface.surfaceFormat = *selectedFormat;
    } else {
        printf("Preferred surface format and colour space was not found. Defaulting to first available option\n");
        m_surface.surfaceFormat = formats[0];
    }
    printf("Using output render colour format %s with colour space %s\n", vk::to_string(m_surface.surfaceFormat.format).c_str(), vk::to_string(m_surface.surfaceFormat.colorSpace).c_str());

    std::vector<vk::PresentModeKHR> presentModes = m_device.physicalDevice->getSurfacePresentModesKHR(**m_surface.surface);

    if (presentModes.empty()) {
        printf("Device \"%s\" supports no present modes\n", m_device.physicalDevice->getProperties().deviceName.data());
        return false;
    }

    auto it = std::find(presentModes.begin(), presentModes.end(), m_preferredPresentMode);
    if (it != presentModes.end()) {
        m_surface.presentMode = *it;
    } else {
        m_surface.presentMode = presentModes.at(0);
        printf("Preferred surface present mode %s was not found. Defaulting to %s\n", vk::to_string(m_preferredPresentMode).c_str(), vk::to_string(m_surface.presentMode).c_str());
    }

    std::vector<vk::Format> depthFormats = {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD32Sfloat};
    m_surface.depthFormat = ImageUtil::selectSupportedFormat(getPhysicalDevice(), depthFormats, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    if (m_surface.depthFormat == vk::Format::eUndefined) {
        std::string formatsStr = "";
        for (int i = 0; i < depthFormats.size(); ++i)
            formatsStr += (i > 0 ? ", " : "") + vk::to_string(depthFormats[0]);
        printf("Requested depth formats [%s] but none were supported\n", formatsStr.c_str());
        return false;
    }

    return true;
}

bool GraphicsManager::recreateSwapchain() {
    PROFILE_SCOPE("GraphicsManager::recreateSwapchain")
    m_recreateSwapchain = false;

    m_device.device->waitIdle();

    m_swapchain.commandBuffers.clear();
    for (int i = 0; i < CONCURRENT_FRAMES; ++i)
        if (m_commandPool->hasCommandBuffer("swapchain_cmd" + std::to_string(i)))
            m_commandPool->freeCommandBuffer("swapchain_cmd" + std::to_string(i));

    m_swapchain.framebuffers.clear();
    m_swapchain.imageViews.clear();
    //delete m_pipeline;
    m_swapchain.swapchain.reset();

    initSurfaceDetails();

    uint32_t imageCount = m_swapchainBufferMode == SwapchainBufferMode_TripleBuffer ? 3 : 2;
    imageCount = glm::clamp(imageCount, m_surface.capabilities.minImageCount, m_surface.capabilities.maxImageCount);

    if (m_surface.capabilities.maxImageCount > 0) {
        imageCount = glm::min(imageCount, m_surface.capabilities.maxImageCount);
    }

    vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eColorAttachment;
    if (m_directImagePresentEnabled)
        imageUsageFlags |= vk::ImageUsageFlagBits::eTransferDst;
    if (m_swapchainImageSampled)
        imageUsageFlags |= vk::ImageUsageFlagBits::eSampled;

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.setSurface(**m_surface.surface);
    createInfo.setMinImageCount(imageCount);
    createInfo.setImageFormat(m_surface.surfaceFormat.format);
    createInfo.setImageColorSpace(m_surface.surfaceFormat.colorSpace);
    createInfo.setImageExtent(m_swapchain.imageExtent);
    createInfo.setImageArrayLayers(1);
    createInfo.setImageUsage(imageUsageFlags);

    std::vector<uint32_t> queueFamilyIndices = {m_queues.queueFamilies.graphicsQueueFamilyIndex.value(), m_queues.queueFamilies.presentQueueFamilyIndex.value()};

    if (m_queues.queueFamilies.graphicsQueueFamilyIndex != m_queues.queueFamilies.presentQueueFamilyIndex) {
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
    //createInfo.setOldSwapchain(nullptr);

    m_swapchain.swapchain = std::make_unique<vkr::SwapchainKHR>(*m_device.device, createInfo);

    if (!createSwapchainImages()) {
        return false;
    }

    if (!createSwapchainFramebuffers()) {
        return false;
    }

    m_swapchain.commandBuffers.resize(CONCURRENT_FRAMES);
    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        std::shared_ptr<vkr::CommandBuffer> commandBuffer = m_commandPool->allocateCommandBuffer("swapchain_cmd" + std::to_string(i), {vk::CommandBufferLevel::ePrimary});
        m_swapchain.commandBuffers[i] = std::move(commandBuffer);
    }


    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    vk::FenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    //const vk::Device& device = **m_device.device;
    //device.createSemaphore(semaphoreCreateInfo);

    for (int i = 0; i < CONCURRENT_FRAMES; ++i) {
        m_swapchain.imageAvailableSemaphores[i].reset();
        m_swapchain.renderFinishedSemaphores[i].reset();
        m_swapchain.inFlightFences[i].reset();

        m_swapchain.imageAvailableSemaphores[i] = std::make_unique<vkr::Semaphore>(*m_device.device, semaphoreCreateInfo);
        m_swapchain.renderFinishedSemaphores[i] = std::make_unique<vkr::Semaphore>(*m_device.device, semaphoreCreateInfo);
        m_swapchain.inFlightFences[i] = std::make_unique<vkr::Fence>(*m_device.device, fenceCreateInfo);
    }

    m_swapchain.currentFrameIndex = 0;

    RecreateSwapchainEvent event{};
    Engine::eventDispatcher()->trigger(&event);

    return true;
}

bool GraphicsManager::createSwapchainImages() {
    m_swapchain.images = m_swapchain.swapchain->getImages();
    m_swapchain.imageViews.resize(m_swapchain.images.size());

    for (int i = 0; i < m_swapchain.images.size(); ++i) {
        ImageViewConfiguration imageViewConfig{};
        imageViewConfig.device = m_device.device;
        imageViewConfig.image = vk::Image(m_swapchain.images[i]);
        imageViewConfig.format = m_surface.surfaceFormat.format;
        m_swapchain.imageViews[i] = std::shared_ptr<ImageView>(ImageView::create(imageViewConfig, "SwapchainPresentImageView"));
    }

    m_swapchain.imagesInFlight.clear();
    m_swapchain.imagesInFlight.resize(m_swapchain.images.size(), VK_NULL_HANDLE);

    return true;
}

bool GraphicsManager::createSwapchainFramebuffers() {
    m_swapchain.framebuffers.resize(m_swapchain.imageViews.size(), nullptr);

    FramebufferConfiguration framebufferConfig{};
    framebufferConfig.device = m_device.device;
    framebufferConfig.setRenderPass(m_renderPass.get());
    framebufferConfig.setSize(m_swapchain.imageExtent);

    for (int i = 0; i < m_swapchain.imageViews.size(); ++i) {
        framebufferConfig.attachments = {
                m_swapchain.imageViews[i]->getImageView(),
        };

        m_swapchain.framebuffers[i] = std::shared_ptr<Framebuffer>(Framebuffer::create(framebufferConfig, "SwapchainPresentFramebuffer"));
    }
    return true;
}

bool GraphicsManager::createRenderPass() {
    vk::AttachmentDescription colourAttachment;
    colourAttachment.setFormat(getColourFormat());
    colourAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colourAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colourAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colourAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    SubpassConfiguration subpassConfiguration;
    subpassConfiguration.addColourAttachment(vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal));
    std::array<vk::SubpassDependency, 1> subpassDependencies;
    subpassDependencies[0].setSrcSubpass(VK_SUBPASS_EXTERNAL);
    subpassDependencies[0].setDstSubpass(0);
    subpassDependencies[0].setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependencies[0].setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    subpassDependencies[0].setSrcAccessMask({});
    subpassDependencies[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    RenderPassConfiguration renderPassConfig{};
    renderPassConfig.device = m_device.device;
    renderPassConfig.addAttachment(colourAttachment);
    renderPassConfig.addSubpass(subpassConfiguration);
    renderPassConfig.setSubpassDependencies(subpassDependencies);

    m_renderPass = SharedResource<RenderPass>(RenderPass::create(renderPassConfig, "GraphicsManager-PresentRenderPass"), "GraphicsManager-PresentRenderPass");
    return m_renderPass != nullptr;
}

void GraphicsManager::shutdownGraphics() {
    ShutdownGraphicsEvent event{};
    Engine::eventDispatcher()->trigger(&event);
}

bool GraphicsManager::beginFrame() {
    PROFILE_SCOPE("GraphicsManager::beginFrame")

    m_debugInfo.reset();

    if (m_recreateSwapchain) {
        m_resolutionChanged = false;
        bool recreated = recreateSwapchain();
#if _DEBUG
        if (!recreated) {
            printf("Failed to recreate swapchain\n");
            assert(false);
        }
#endif
        return false; // Skip this frame
    }

    PROFILE_REGION("Wait for frame fence")

    const vk::Device& device = **m_device.device;
    const vk::Semaphore& imageAvailableSemaphore = **m_swapchain.imageAvailableSemaphores[m_swapchain.currentFrameIndex];
    const vk::Fence& frameFence = **m_swapchain.inFlightFences[m_swapchain.currentFrameIndex];
    const vk::SwapchainKHR& swapchain = **m_swapchain.swapchain;

    vk::Result result = m_device.device->waitForFences({frameFence}, true, UINT64_MAX);
    assert(result == vk::Result::eSuccess);

//    PROFILE_REGION("Reset frame fence")
//    m_device.device->resetFences({ frameFence });

    if (m_swapchain.imageExtent.width != Application::instance()->getWindowSize().x ||
        m_swapchain.imageExtent.height != Application::instance()->getWindowSize().y) {
        m_recreateSwapchain = true;
        m_resolutionChanged = true;
        return false;
    }

    PROFILE_REGION("Acquire next image")
    vk::AcquireNextImageInfoKHR acquireInfo;
    acquireInfo.setDeviceMask(1); // I can't find any information in the documentation about deviceMask... Presumably setting bit 0 means select device 0
    acquireInfo.setSwapchain(swapchain);
    acquireInfo.setTimeout(UINT64_MAX);
    acquireInfo.setSemaphore(imageAvailableSemaphore);
    acquireInfo.setFence(VK_NULL_HANDLE);
    vk::ResultValue<uint32_t> acquireNextImageResult = device.acquireNextImage2KHR(acquireInfo);
    if (acquireNextImageResult.result == vk::Result::eErrorOutOfDateKHR || acquireNextImageResult.result == vk::Result::eSuboptimalKHR) {
        m_recreateSwapchain = true;
        return false;

    } else if (acquireNextImageResult.result != vk::Result::eSuccess) {
        return false;
    }

    m_swapchain.prevImageIndex = m_swapchain.currentImageIndex;
    m_swapchain.currentImageIndex = acquireNextImageResult.value;
//    if (m_swapchain.prevImageIndex == m_swapchain.currentImageIndex) {
//        m_swapchain.prevImageIndex = (m_swapchain.currentImageIndex + m_swapchain.imageViews.size() - 1) % m_swapchain.imageViews.size();
//    }

    const vk::CommandBuffer& commandBuffer = getCurrentCommandBuffer();
    const Framebuffer* framebuffer = getCurrentFramebuffer();

    if (!commandBuffer || !framebuffer) {
        return false;
    }

    vk::CommandBufferBeginInfo beginInfo;

    PROFILE_REGION("Begin command buffer");
    commandBuffer.begin(beginInfo);

    Profiler::beginGraphicsFrame();
    PROFILE_BEGIN_GPU_CMD("Frame", commandBuffer);

    return true;
}

void GraphicsManager::endFrame() {
    PROFILE_SCOPE("GraphicsManager::endFrame")
    const vk::Device& device = **m_device.device;
    const vk::Semaphore& imageAvailableSemaphore = **m_swapchain.imageAvailableSemaphores[m_swapchain.currentFrameIndex];
    const vk::Semaphore& renderFinishedSemaphore = **m_swapchain.renderFinishedSemaphores[m_swapchain.currentFrameIndex];
    const vk::Fence& frameFence = **m_swapchain.inFlightFences[m_swapchain.currentFrameIndex];
    const vk::SwapchainKHR& swapchain = **m_swapchain.swapchain;
    const vk::CommandBuffer& commandBuffer = getCurrentCommandBuffer();

    PROFILE_REGION("End command buffer")
    PROFILE_END_GPU_CMD(commandBuffer);

    commandBuffer.end();

    PROFILE_REGION("Wait for image fence")

    if (m_swapchain.imagesInFlight[m_swapchain.currentImageIndex]) {
        vk::Result result = device.waitForFences({m_swapchain.imagesInFlight[m_swapchain.currentImageIndex]}, true, UINT64_MAX);
        assert(result == vk::Result::eSuccess);
    }
    m_swapchain.imagesInFlight[m_swapchain.currentImageIndex] = frameFence;

    PROFILE_REGION("Reset frame fence")
    m_device.device->resetFences({frameFence});

    PROFILE_REGION("Submit queues")

    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

    std::vector<vk::SubmitInfo> submitInfos;
    vk::SubmitInfo& submitInfo = submitInfos.emplace_back();
    submitInfo.setWaitSemaphoreCount(1);
    submitInfo.setPWaitSemaphores(&imageAvailableSemaphore);
    submitInfo.setPWaitDstStageMask(waitStages);
    submitInfo.setCommandBufferCount(1);
    submitInfo.setPCommandBuffers(&commandBuffer);
    submitInfo.setSignalSemaphoreCount(1);
    submitInfo.setPSignalSemaphores(&renderFinishedSemaphore);

    const vk::Queue& queue = **m_queues.queues[QUEUE_GRAPHICS_MAIN];

    // Submit the queue, signal frameFence once the commands are executed
    queue.submit(submitInfos, frameFence);

    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setPWaitSemaphores(&renderFinishedSemaphore);
    presentInfo.setSwapchainCount(1);
    presentInfo.setPSwapchains(&swapchain);
    presentInfo.setPImageIndices(&m_swapchain.currentImageIndex);

    PROFILE_REGION("Present queue")
    vk::Result result = queue.presentKHR(presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        m_recreateSwapchain = true;
        return;
    }

    m_swapchain.currentFrameIndex = (m_swapchain.currentFrameIndex + 1) % CONCURRENT_FRAMES;

    Profiler::endGraphicsFrame();

    if (m_flushRendering) {
        m_flushRendering = false;
        Engine::graphics()->getDevice()->waitIdle();
        FlushRenderingEvent event{};
//        printf("======== ======== DISPATCH FlushRenderingEvent ======== ========\n\n");
        Engine::eventDispatcher()->trigger(&event);
    }
}

void GraphicsManager::flushRendering() {
    // At the end of the current frame, all rendering commands will be flushed, and a FlushRenderingEvent will be triggered
    m_flushRendering = true;
}

void GraphicsManager::presentImageDirect(const vk::CommandBuffer& commandBuffer, const vk::Image& image, const vk::ImageLayout& imageLayout) {
    PROFILE_SCOPE("GraphicsManager::presentImageDirect");
    PROFILE_BEGIN_GPU_CMD("GraphicsManager::presentImageDirect", commandBuffer);
    VkImage currentImage = m_swapchain.images[m_swapchain.currentImageIndex];
    vk::ImageCopy region{};

    vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
    ImageUtil::transitionLayout(commandBuffer, currentImage, subresourceRange, ImageTransition::FromAny(), ImageTransition::TransferDst());

    region.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    region.srcOffset = vk::Offset3D(0, 0, 0);
    region.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
    region.dstOffset = vk::Offset3D(0, 0, 0);
    region.extent = vk::Extent3D(m_swapchain.imageExtent.width, m_swapchain.imageExtent.height, 1);

    commandBuffer.copyImage(image, imageLayout, currentImage, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    ImageUtil::transitionLayout(commandBuffer, currentImage, subresourceRange, ImageTransition::TransferDst(), ImageTransition::PresentSrc());
    PROFILE_END_GPU_CMD(commandBuffer);
}

const vk::Instance& GraphicsManager::getInstance() const {
    return **m_instance;
}

const SharedResource<vkr::Device>& GraphicsManager::getDevice() const {
    return m_device.device;
}

const vk::PhysicalDevice& GraphicsManager::getPhysicalDevice() const {
    return **m_device.physicalDevice;
}

const vk::PhysicalDeviceMemoryProperties& GraphicsManager::getDeviceMemoryProperties() const {
    return m_device.memoryProperties;
}

const vk::PhysicalDeviceProperties& GraphicsManager::getPhysicalDeviceProperties() const {
    return m_device.physicalDeviceProperties;
}

const vk::PhysicalDeviceLimits& GraphicsManager::getPhysicalDeviceLimits() const {
    return m_device.physicalDeviceProperties.limits;
}

vk::DeviceSize GraphicsManager::getAlignedUniformBufferOffset(const vk::DeviceSize& offset) {
    const vk::DeviceSize& minOffsetAlignment = Engine::graphics()->getPhysicalDeviceLimits().minUniformBufferOffsetAlignment;
    return CEIL_TO_MULTIPLE(offset, minOffsetAlignment);
}

uint32_t GraphicsManager::getPreviousFrameIndex() const {
    return (m_swapchain.currentFrameIndex + CONCURRENT_FRAMES - 1) % CONCURRENT_FRAMES;
}

const uint32_t& GraphicsManager::getCurrentFrameIndex() const {
    return m_swapchain.currentFrameIndex;
}

const vk::CommandBuffer& GraphicsManager::getCurrentCommandBuffer() const {
    return **m_swapchain.commandBuffers[m_swapchain.currentFrameIndex];
}

const Framebuffer* GraphicsManager::getCurrentFramebuffer() const {
    return m_swapchain.framebuffers[m_swapchain.currentImageIndex].get();
}

const ImageView* GraphicsManager::getPreviousFrameImageView() const {
    if (m_swapchain.prevImageIndex >= m_swapchain.imageViews.size()) {
        return nullptr;
    }
    return m_swapchain.imageViews[m_swapchain.prevImageIndex].get();
}

const std::shared_ptr<vkr::Queue>& GraphicsManager::getQueue(const std::string& name) const {
    return m_queues.queues.at(name);
}

bool GraphicsManager::hasQueue(const std::string& name) const {
    return m_queues.queues.count(name) > 0;
}

const uint32_t& GraphicsManager::getGraphicsQueueFamilyIndex() const {
    return m_queues.queueFamilies.graphicsQueueFamilyIndex.value();
}

const uint32_t& GraphicsManager::getComputeQueueFamilyIndex() const {
    return m_queues.queueFamilies.computeQueueFamilyIndex.value();
}

const uint32_t& GraphicsManager::getTransferQueueFamilyIndex() const {
    return m_queues.queueFamilies.transferQueueFamilyIndex.value();
}

const uint32_t& GraphicsManager::getSparseBindingQueueFamilyIndex() const {
    return m_queues.queueFamilies.sparseBindingQueueFamilyIndex.value();
}

const uint32_t& GraphicsManager::getProtectedQueueFamilyIndex() const {
    return m_queues.queueFamilies.protectedQueueFamilyIndex.value();
}

const uint32_t& GraphicsManager::getPresentQueueFamilyIndex() const {
    return m_queues.queueFamilies.presentQueueFamilyIndex.value();
}

const vk::CommandBuffer& GraphicsManager::beginOneTimeCommandBuffer() {
    CommandBufferConfiguration commandBufferConfig;
    commandBufferConfig.level = vk::CommandBufferLevel::ePrimary;
//    const vk::CommandBuffer& commandBuffer = **Engine::graphics()->commandPool()->getOrCreateCommandBuffer("one_time_cmd", commandBufferConfig);
    const vk::CommandBuffer& commandBuffer = commandPool()->getTemporaryCommandBuffer("one_time_cmd", commandBufferConfig);

    vk::CommandBufferBeginInfo commandBeginInfo;
    commandBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    commandBuffer.begin(commandBeginInfo);
    return commandBuffer;
}

void GraphicsManager::endOneTimeCommandBuffer(const vk::CommandBuffer& commandBuffer, const vk::Queue& queue) {
    commandBuffer.end();

    const vk::Fence& fence = commandPool()->releaseTemporaryCommandBufferFence(commandBuffer);

    vk::SubmitInfo queueSubmitInfo{};
    queueSubmitInfo.setCommandBufferCount(1);
    queueSubmitInfo.setPCommandBuffers(&commandBuffer);
    vk::Result result = queue.submit(1, &queueSubmitInfo, fence);
    assert(result == vk::Result::eSuccess);

}

void GraphicsManager::endOneTimeCommandBuffer(const vk::CommandBuffer& commandBuffer, const std::shared_ptr<vkr::Queue>& queue) {
    endOneTimeCommandBuffer(commandBuffer, **queue);
}


void GraphicsManager::setPreferredPresentMode(vk::PresentModeKHR presentMode) {
    if (m_preferredPresentMode != presentMode) {
        m_preferredPresentMode = presentMode;
        m_recreateSwapchain = true;
    }
}

bool GraphicsManager::didResolutionChange() const {
    return m_resolutionChanged;
}

const bool& GraphicsManager::isDirectImagePresentEnabled() const {
    return m_directImagePresentEnabled;
}

void GraphicsManager::setDirectImagePresentEnabled(const bool& directImagePresentEnabled) {
    if (m_directImagePresentEnabled != directImagePresentEnabled) {
        m_directImagePresentEnabled = directImagePresentEnabled;
        m_recreateSwapchain = true;
    }
}

const bool& GraphicsManager::isSwapchainImageSampled() const {
    return m_swapchainImageSampled;
}

void GraphicsManager::setSwapchainImageSampled(const bool& swapchainImageSampled) {
    if (m_swapchainImageSampled != swapchainImageSampled) {
        m_swapchainImageSampled = swapchainImageSampled;
        m_recreateSwapchain = true;
    }
}

DebugUtils::RenderInfo& GraphicsManager::debugInfo() {
    return m_debugInfo;
}

const DebugUtils::RenderInfo& GraphicsManager::getDebugInfo() const {
    return m_debugInfo;
}

ResourceId GraphicsManager::nextResourceId() {
    return ++s_nextResourceID;
}

const SharedResource<RenderPass>& GraphicsManager::renderPass() {
    return m_renderPass;
}

const SharedResource<CommandPool>& GraphicsManager::commandPool() {
    return m_commandPool;
}

const SharedResource<DescriptorPool>& GraphicsManager::descriptorPool() {
    return m_descriptorPool;
}

glm::ivec2 GraphicsManager::getResolution() const {
    return glm::ivec2(m_swapchain.imageExtent.width, m_swapchain.imageExtent.height);
}

glm::vec2 GraphicsManager::getNormalizedPixelSize() const {
    return glm::vec2(1.0F) / glm::vec2(getResolution());
}

float GraphicsManager::getAspectRatio() const {
    return (float)m_swapchain.imageExtent.width / (float)m_swapchain.imageExtent.height;
}

const vk::Extent2D& GraphicsManager::getImageExtent() const {
    return m_swapchain.imageExtent;
}

vk::Format GraphicsManager::getColourFormat() const {
    return m_surface.surfaceFormat.format;
}

vk::Format GraphicsManager::getDepthFormat() const {
    return m_surface.depthFormat;
}

DeviceMemoryManager& GraphicsManager::memory() {
    return *m_memory;
}

vk::ColorSpaceKHR GraphicsManager::getColourSpace() const {
    return m_surface.surfaceFormat.colorSpace;
}

void GraphicsManager::setObjectName(const vk::Device& device, const uint64_t& objectHandle, const vk::ObjectType& objectType, const char* objectName) {
    if (objectName != nullptr) {
        vk::DebugUtilsObjectNameInfoEXT objectNameInfo{};
        objectNameInfo.objectHandle = objectHandle;
        objectNameInfo.objectType = objectType;
        objectNameInfo.pObjectName = objectName;
        device.setDebugUtilsObjectNameEXT(objectNameInfo);
    }
}

void GraphicsManager::setObjectName(const vk::Device& device, const uint64_t& objectHandle, const vk::ObjectType& objectType, const std::string& objectName) {
    setObjectName(device, objectHandle, objectType, objectName.c_str());
}

void GraphicsManager::insertQueueDebugLabel(const vk::Queue& queue, const char* name) {
    if (name != nullptr) {
        static std::unordered_map<std::string, std::array<float, 4>> s_colours;
        auto colour = Util::mapComputeIfAbsent(s_colours, std::string(name), [](const std::string& key) {
            return Util::randomArray<float, 4>(0.1F, 0.95F);
        });
        vk::DebugUtilsLabelEXT label{};
        label.pLabelName = name;
        label.setColor(colour);
        queue.insertDebugUtilsLabelEXT(label);
    }
}

void GraphicsManager::beginQueueDebugLabel(const vk::Queue& queue, const char* name) {
    if (name != nullptr) {
        static std::unordered_map<std::string, std::array<float, 4>> s_colours;
        auto colour = Util::mapComputeIfAbsent(s_colours, std::string(name), [](const std::string& key) {
            return Util::randomArray<float, 4>(0.1F, 0.95F);
        });
        vk::DebugUtilsLabelEXT label{};
        label.pLabelName = name;
        label.setColor(colour);
        queue.beginDebugUtilsLabelEXT(label);
    }
}

void GraphicsManager::endQueueDebugLabel(const vk::Queue& queue) {
    queue.endDebugUtilsLabelEXT();
}

void GraphicsManager::insertCmdDebugLabel(const vk::CommandBuffer& commandBuffer, const char* name) {
    if (name != nullptr) {
        static std::unordered_map<std::string, std::array<float, 4>> s_colours;
        auto colour = Util::mapComputeIfAbsent(s_colours, std::string(name), [](const std::string& key) {
            return Util::randomArray<float, 4>(0.1F, 0.95F);
        });
        vk::DebugUtilsLabelEXT label{};
        label.pLabelName = name;
        label.setColor(colour);
        commandBuffer.insertDebugUtilsLabelEXT(label);
    }
}

void GraphicsManager::beginCmdDebugLabel(const vk::CommandBuffer& commandBuffer, const char* name) {
    assert(name != nullptr);
    static std::unordered_map<std::string, std::array<float, 4>> s_colours;
    auto colour = Util::mapComputeIfAbsent(s_colours, std::string(name), [](const std::string& key) {
        return Util::randomArray<float, 4>(0.1F, 0.95F);
    });
    vk::DebugUtilsLabelEXT label{};
    label.pLabelName = name;
    label.setColor(colour);
    commandBuffer.beginDebugUtilsLabelEXT(label);
}

void GraphicsManager::endCmdDebugLabel(const vk::CommandBuffer& commandBuffer) {
    commandBuffer.endDebugUtilsLabelEXT();
}

bool GraphicsManager::doAbortOnVulkanError() {
    return m_abortOnVulkanError;
}

void GraphicsManager::setAbortOnVulkanError(const bool& abortOnVulkanError) {
    m_abortOnVulkanError = abortOnVulkanError;
}
