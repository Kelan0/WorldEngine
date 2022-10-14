#ifndef WORLDENGINE_VULKAN_H
#define WORLDENGINE_VULKAN_H

#include <vulkan/vulkan.h>

void loadVulkanInstanceExtensions(VkInstance device);

void loadVulkanDeviceExtensions(VkDevice device);

#if defined(VK_EXT_extended_dynamic_state)
extern PFN_vkCmdSetCullModeEXT ptr_vkCmdSetCullModeEXT;
extern PFN_vkCmdSetFrontFaceEXT ptr_vkCmdSetFrontFaceEXT;
extern PFN_vkCmdSetPrimitiveTopologyEXT ptr_vkCmdSetPrimitiveTopologyEXT;
extern PFN_vkCmdSetViewportWithCountEXT ptr_vkCmdSetViewportWithCountEXT;
extern PFN_vkCmdSetScissorWithCountEXT ptr_vkCmdSetScissorWithCountEXT;
extern PFN_vkCmdBindVertexBuffers2EXT ptr_vkCmdBindVertexBuffers2EXT;
extern PFN_vkCmdSetDepthTestEnableEXT ptr_vkCmdSetDepthTestEnableEXT;
extern PFN_vkCmdSetDepthWriteEnableEXT ptr_vkCmdSetDepthWriteEnableEXT;
extern PFN_vkCmdSetDepthCompareOpEXT ptr_vkCmdSetDepthCompareOpEXT;
extern PFN_vkCmdSetDepthBoundsTestEnableEXT ptr_vkCmdSetDepthBoundsTestEnableEXT;
extern PFN_vkCmdSetStencilTestEnableEXT ptr_vkCmdSetStencilTestEnableEXT;
extern PFN_vkCmdSetStencilOpEXT ptr_vkCmdSetStencilOpEXT;
#endif /* defined(VK_EXT_extended_dynamic_state) */

#if defined(VK_EXT_extended_dynamic_state2)
extern PFN_vkCmdSetPatchControlPointsEXT ptr_vkCmdSetPatchControlPointsEXT;
extern PFN_vkCmdSetRasterizerDiscardEnableEXT ptr_vkCmdSetRasterizerDiscardEnableEXT;
extern PFN_vkCmdSetDepthBiasEnableEXT ptr_vkCmdSetDepthBiasEnableEXT;
extern PFN_vkCmdSetLogicOpEXT ptr_vkCmdSetLogicOpEXT;
extern PFN_vkCmdSetPrimitiveRestartEnableEXT ptr_vkCmdSetPrimitiveRestartEnableEXT;
#endif /* defined(VK_EXT_extended_dynamic_state2) */


#if defined(VK_EXT_line_rasterization)
extern PFN_vkCmdSetLineStippleEXT ptr_vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */

#if defined(VK_EXT_sample_locations)
extern PFN_vkCmdSetSampleLocationsEXT ptr_vkCmdSetSampleLocationsEXT;
extern PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */

#if defined(VK_EXT_vertex_input_dynamic_state)
extern PFN_vkCmdSetVertexInputEXT ptr_vkCmdSetVertexInputEXT;
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */

#if defined(VK_EXT_color_write_enable)
extern PFN_vkCmdSetColorWriteEnableEXT ptr_vkCmdSetColorWriteEnableEXT;
#endif /* defined(VK_EXT_color_write_enable) */

#if defined(VK_EXT_debug_utils)
extern PFN_vkSetDebugUtilsObjectNameEXT ptr_vkSetDebugUtilsObjectNameEXT;
extern PFN_vkSetDebugUtilsObjectTagEXT ptr_vkSetDebugUtilsObjectTagEXT;
extern PFN_vkQueueBeginDebugUtilsLabelEXT ptr_vkQueueBeginDebugUtilsLabelEXT;
extern PFN_vkQueueEndDebugUtilsLabelEXT ptr_vkQueueEndDebugUtilsLabelEXT;
extern PFN_vkQueueInsertDebugUtilsLabelEXT ptr_vkQueueInsertDebugUtilsLabelEXT;
extern PFN_vkCmdBeginDebugUtilsLabelEXT ptr_vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT ptr_vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT ptr_vkCmdInsertDebugUtilsLabelEXT;
extern PFN_vkCreateDebugUtilsMessengerEXT ptr_vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT ptr_vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkSubmitDebugUtilsMessageEXT ptr_vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */

#endif //WORLDENGINE_VULKAN_H
