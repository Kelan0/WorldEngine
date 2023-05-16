#include "core/Vulkan.h"
#include "core/util/Logger.h"
#include <iostream>
#include <cassert>

// Why do we need to load extension methods like this????? this is so tedious :(


#define LOAD_DEVICE_FUNCTION(functionName) \
    ptr_##functionName = (PFN_##functionName) vkGetDeviceProcAddr(device, #functionName); \
    if (ptr_##functionName == nullptr)     \
        LOG_ERROR("Failed to load vulkan device extension function \"" #functionName "\""); // \
//    assert(ptr_##functionName != nullptr);

#if defined(VK_EXT_extended_dynamic_state)
PFN_vkCmdSetCullModeEXT ptr_vkCmdSetCullModeEXT;
PFN_vkCmdSetFrontFaceEXT ptr_vkCmdSetFrontFaceEXT;
PFN_vkCmdSetPrimitiveTopologyEXT ptr_vkCmdSetPrimitiveTopologyEXT;
PFN_vkCmdSetViewportWithCountEXT ptr_vkCmdSetViewportWithCountEXT;
PFN_vkCmdSetScissorWithCountEXT ptr_vkCmdSetScissorWithCountEXT;
PFN_vkCmdBindVertexBuffers2EXT ptr_vkCmdBindVertexBuffers2EXT;
PFN_vkCmdSetDepthTestEnableEXT ptr_vkCmdSetDepthTestEnableEXT;
PFN_vkCmdSetDepthWriteEnableEXT ptr_vkCmdSetDepthWriteEnableEXT;
PFN_vkCmdSetDepthCompareOpEXT ptr_vkCmdSetDepthCompareOpEXT;
PFN_vkCmdSetDepthBoundsTestEnableEXT ptr_vkCmdSetDepthBoundsTestEnableEXT;
PFN_vkCmdSetStencilTestEnableEXT ptr_vkCmdSetStencilTestEnableEXT;
PFN_vkCmdSetStencilOpEXT ptr_vkCmdSetStencilOpEXT;
#endif /* defined(VK_EXT_extended_dynamic_state) */

#if defined(VK_EXT_extended_dynamic_state2)
PFN_vkCmdSetPatchControlPointsEXT ptr_vkCmdSetPatchControlPointsEXT;
PFN_vkCmdSetRasterizerDiscardEnableEXT ptr_vkCmdSetRasterizerDiscardEnableEXT;
PFN_vkCmdSetDepthBiasEnableEXT ptr_vkCmdSetDepthBiasEnableEXT;
PFN_vkCmdSetLogicOpEXT ptr_vkCmdSetLogicOpEXT;
PFN_vkCmdSetPrimitiveRestartEnableEXT ptr_vkCmdSetPrimitiveRestartEnableEXT;
#endif /* defined(VK_EXT_extended_dynamic_state2) */


#if defined(VK_EXT_line_rasterization)
PFN_vkCmdSetLineStippleEXT ptr_vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */

#if defined(VK_EXT_sample_locations)
PFN_vkCmdSetSampleLocationsEXT ptr_vkCmdSetSampleLocationsEXT;
PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */

#if defined(VK_EXT_vertex_input_dynamic_state)
PFN_vkCmdSetVertexInputEXT ptr_vkCmdSetVertexInputEXT;
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */

#if defined(VK_EXT_color_write_enable)
PFN_vkCmdSetColorWriteEnableEXT ptr_vkCmdSetColorWriteEnableEXT;
#endif /* defined(VK_EXT_color_write_enable) */

#if defined(VK_EXT_debug_utils)
PFN_vkSetDebugUtilsObjectNameEXT ptr_vkSetDebugUtilsObjectNameEXT;
PFN_vkSetDebugUtilsObjectTagEXT ptr_vkSetDebugUtilsObjectTagEXT;
PFN_vkQueueBeginDebugUtilsLabelEXT ptr_vkQueueBeginDebugUtilsLabelEXT;
PFN_vkQueueEndDebugUtilsLabelEXT ptr_vkQueueEndDebugUtilsLabelEXT;
PFN_vkQueueInsertDebugUtilsLabelEXT ptr_vkQueueInsertDebugUtilsLabelEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT ptr_vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT ptr_vkCmdEndDebugUtilsLabelEXT;
PFN_vkCmdInsertDebugUtilsLabelEXT ptr_vkCmdInsertDebugUtilsLabelEXT;
PFN_vkCreateDebugUtilsMessengerEXT ptr_vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT ptr_vkDestroyDebugUtilsMessengerEXT;
PFN_vkSubmitDebugUtilsMessageEXT ptr_vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */




void loadVulkanInstanceExtensions(VkInstance device) {
}

void loadVulkanDeviceExtensions(VkDevice device) {
#if defined(VK_EXT_extended_dynamic_state)
    LOAD_DEVICE_FUNCTION(vkCmdSetCullModeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetFrontFaceEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetPrimitiveTopologyEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetViewportWithCountEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetScissorWithCountEXT);
    LOAD_DEVICE_FUNCTION(vkCmdBindVertexBuffers2EXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthTestEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthWriteEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthCompareOpEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthBoundsTestEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetStencilTestEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetStencilOpEXT);
#endif /* defined(VK_EXT_extended_dynamic_state) */

#if defined(VK_EXT_line_rasterization)
    LOAD_DEVICE_FUNCTION(vkCmdSetLineStippleEXT)
#endif /* defined(VK_EXT_line_rasterization) */

#if defined(VK_EXT_sample_locations)
    LOAD_DEVICE_FUNCTION(vkCmdSetSampleLocationsEXT);
    LOAD_DEVICE_FUNCTION(vkGetPhysicalDeviceMultisamplePropertiesEXT);
#endif /* defined(VK_EXT_sample_locations) */

#if defined(VK_EXT_extended_dynamic_state2)
    LOAD_DEVICE_FUNCTION(vkCmdSetPatchControlPointsEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetRasterizerDiscardEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthBiasEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetLogicOpEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetPrimitiveRestartEnableEXT);
#endif /* defined(VK_EXT_extended_dynamic_state2) */

#if defined(VK_EXT_vertex_input_dynamic_state)
    LOAD_DEVICE_FUNCTION(vkCmdSetVertexInputEXT);
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */

#if defined(VK_EXT_color_write_enable)
    LOAD_DEVICE_FUNCTION(vkCmdSetColorWriteEnableEXT);
#endif /* defined(VK_EXT_color_write_enable) */

#if defined(VK_EXT_debug_utils)
    LOAD_DEVICE_FUNCTION(vkSetDebugUtilsObjectNameEXT);
    LOAD_DEVICE_FUNCTION(vkSetDebugUtilsObjectTagEXT);
    LOAD_DEVICE_FUNCTION(vkQueueBeginDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkQueueEndDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkQueueInsertDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkCmdBeginDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkCmdEndDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkCmdInsertDebugUtilsLabelEXT);
    LOAD_DEVICE_FUNCTION(vkCreateDebugUtilsMessengerEXT);
    LOAD_DEVICE_FUNCTION(vkDestroyDebugUtilsMessengerEXT);
    LOAD_DEVICE_FUNCTION(vkSubmitDebugUtilsMessageEXT);
#endif /* defined(VK_EXT_debug_utils) */
}




#if 1
#if defined(VK_EXT_extended_dynamic_state)
void vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) { ptr_vkCmdSetCullModeEXT(commandBuffer, cullMode); }
void vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) { ptr_vkCmdSetFrontFaceEXT(commandBuffer, frontFace); }
void vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) { ptr_vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology); }
void vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) { ptr_vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports); }
void vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) { ptr_vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors); }
void vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) { ptr_vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides); }
void vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) { ptr_vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable); }
void vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) { ptr_vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable); }
void vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) { ptr_vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp); }
void vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) { ptr_vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable); }
void vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) { ptr_vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable); }
void vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) { ptr_vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp); }
#endif /* defined(VK_EXT_extended_dynamic_state) */

#if defined(VK_EXT_line_rasterization)
void vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) { ptr_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern); }
#endif /* defined(VK_EXT_line_rasterization) */

#if defined(VK_EXT_sample_locations)
void vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) { ptr_vkCmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo); }
void vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) { ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, pMultisampleProperties); }
#endif /* defined(VK_EXT_sample_locations) */

#if defined(VK_EXT_extended_dynamic_state2)
void vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) { ptr_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints); }
void vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) { ptr_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable); }
void vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) { ptr_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable); }
void vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) { ptr_vkCmdSetLogicOpEXT(commandBuffer, logicOp); }
void vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) { ptr_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable); }
#endif /* defined(VK_EXT_extended_dynamic_state2) */

#if defined(VK_EXT_vertex_input_dynamic_state)
void vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) { ptr_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions); }
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */

#if defined(VK_EXT_color_write_enable)
void vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) { ptr_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables); }
#endif /* defined(VK_EXT_color_write_enable) */

#if defined(VK_EXT_debug_utils)
VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) { return ptr_vkSetDebugUtilsObjectNameEXT(device, pNameInfo); }
VkResult vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) { return ptr_vkSetDebugUtilsObjectTagEXT(device, pTagInfo); }
void vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) { ptr_vkQueueBeginDebugUtilsLabelEXT(queue, pLabelInfo); }
void vkQueueEndDebugUtilsLabelEXT(VkQueue queue) { ptr_vkQueueEndDebugUtilsLabelEXT(queue); }
void vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) { ptr_vkQueueInsertDebugUtilsLabelEXT(queue, pLabelInfo); }
void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) { ptr_vkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo); }
void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) { ptr_vkCmdEndDebugUtilsLabelEXT(commandBuffer); }
void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) { ptr_vkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo); }
VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) { return ptr_vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger); }
void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) { ptr_vkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator); }
void vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) { ptr_vkSubmitDebugUtilsMessageEXT(instance, messageSeverity, messageTypes, pCallbackData); }
#endif /* defined(VK_EXT_debug_utils) */
#endif