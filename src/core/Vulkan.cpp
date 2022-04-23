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
