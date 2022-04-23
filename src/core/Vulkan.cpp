#include "core/Vulkan.h"

// Why do we need to load extension methods like this????? this is so tedious :(


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





void loadVulkanInstanceExtensions(VkInstance device) {
}

void loadVulkanDeviceExtensions(VkDevice device) {
#if defined(VK_EXT_extended_dynamic_state)
    ptr_vkCmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT) vkGetDeviceProcAddr(device, "vkCmdSetCullModeEXT");
    ptr_vkCmdSetFrontFaceEXT = (PFN_vkCmdSetFrontFaceEXT) vkGetDeviceProcAddr(device, "vkCmdSetFrontFaceEXT");
    ptr_vkCmdSetPrimitiveTopologyEXT = (PFN_vkCmdSetPrimitiveTopologyEXT) vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveTopologyEXT");
    ptr_vkCmdSetViewportWithCountEXT = (PFN_vkCmdSetViewportWithCountEXT) vkGetDeviceProcAddr(device, "vkCmdSetViewportWithCountEXT");
    ptr_vkCmdSetScissorWithCountEXT = (PFN_vkCmdSetScissorWithCountEXT) vkGetDeviceProcAddr(device, "vkCmdSetScissorWithCountEXT");
    ptr_vkCmdBindVertexBuffers2EXT = (PFN_vkCmdBindVertexBuffers2EXT) vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers2EXT");
    ptr_vkCmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthTestEnableEXT");
    ptr_vkCmdSetDepthWriteEnableEXT = (PFN_vkCmdSetDepthWriteEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthWriteEnableEXT");
    ptr_vkCmdSetDepthCompareOpEXT = (PFN_vkCmdSetDepthCompareOpEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthCompareOpEXT");
    ptr_vkCmdSetDepthBoundsTestEnableEXT = (PFN_vkCmdSetDepthBoundsTestEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthBoundsTestEnableEXT");
    ptr_vkCmdSetStencilTestEnableEXT = (PFN_vkCmdSetStencilTestEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetStencilTestEnableEXT");
    ptr_vkCmdSetStencilOpEXT = (PFN_vkCmdSetStencilOpEXT) vkGetDeviceProcAddr(device, "vkCmdSetStencilOpEXT");
#endif /* defined(VK_EXT_extended_dynamic_state) */

#if defined(VK_EXT_extended_dynamic_state2)
    ptr_vkCmdSetPatchControlPointsEXT = (PFN_vkCmdSetPatchControlPointsEXT) vkGetDeviceProcAddr(device, "vkCmdSetPatchControlPointsEXT");
    ptr_vkCmdSetRasterizerDiscardEnableEXT = (PFN_vkCmdSetRasterizerDiscardEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetRasterizerDiscardEnableEXT");
    ptr_vkCmdSetDepthBiasEnableEXT = (PFN_vkCmdSetDepthBiasEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetDepthBiasEnableEXT");
    ptr_vkCmdSetLogicOpEXT = (PFN_vkCmdSetLogicOpEXT) vkGetDeviceProcAddr(device, "vkCmdSetLogicOpEXT");
    ptr_vkCmdSetPrimitiveRestartEnableEXT = (PFN_vkCmdSetPrimitiveRestartEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetPrimitiveRestartEnableEXT");
#endif /* defined(VK_EXT_extended_dynamic_state2) */

#if defined(VK_EXT_line_rasterization)
    ptr_vkCmdSetLineStippleEXT = (PFN_vkCmdSetLineStippleEXT) vkGetDeviceProcAddr(device, "vkCmdSetLineStippleEXT");
#endif /* defined(VK_EXT_line_rasterization) */

#if defined(VK_EXT_sample_locations)
    ptr_vkCmdSetSampleLocationsEXT = (PFN_vkCmdSetSampleLocationsEXT) vkGetDeviceProcAddr(device, "vkCmdSetSampleLocationsEXT");
    ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT = (PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT) vkGetDeviceProcAddr(device, "vkGetPhysicalDeviceMultisamplePropertiesEXT");
#endif /* defined(VK_EXT_sample_locations) */

#if defined(VK_EXT_vertex_input_dynamic_state)
    ptr_vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT) vkGetDeviceProcAddr(device, "vkCmdSetVertexInputEXT");
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */

#if defined(VK_EXT_color_write_enable)
    ptr_vkCmdSetColorWriteEnableEXT = (PFN_vkCmdSetColorWriteEnableEXT) vkGetDeviceProcAddr(device, "vkCmdSetColorWriteEnableEXT");
#endif /* defined(VK_EXT_color_write_enable) */

}

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

void vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) { ptr_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern); }

void vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) { ptr_vkCmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo); }
void vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) { ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, pMultisampleProperties); }

void vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) { ptr_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions); }

void vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) { ptr_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints); }
void vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) { ptr_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable); }
void vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) { ptr_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable); }
void vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) { ptr_vkCmdSetLogicOpEXT(commandBuffer, logicOp); }
void vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) { ptr_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable); }

void vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) { ptr_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables); }