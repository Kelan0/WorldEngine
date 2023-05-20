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

#if defined(VK_EXT_extended_dynamic_state3)
PFN_vkCmdSetTessellationDomainOriginEXT ptr_vkCmdSetTessellationDomainOriginEXT;
PFN_vkCmdSetDepthClampEnableEXT ptr_vkCmdSetDepthClampEnableEXT;
PFN_vkCmdSetPolygonModeEXT ptr_vkCmdSetPolygonModeEXT;
PFN_vkCmdSetRasterizationSamplesEXT ptr_vkCmdSetRasterizationSamplesEXT;
PFN_vkCmdSetSampleMaskEXT ptr_vkCmdSetSampleMaskEXT;
PFN_vkCmdSetAlphaToCoverageEnableEXT ptr_vkCmdSetAlphaToCoverageEnableEXT;
PFN_vkCmdSetAlphaToOneEnableEXT ptr_vkCmdSetAlphaToOneEnableEXT;
PFN_vkCmdSetLogicOpEnableEXT ptr_vkCmdSetLogicOpEnableEXT;
PFN_vkCmdSetColorBlendEnableEXT ptr_vkCmdSetColorBlendEnableEXT;
PFN_vkCmdSetColorBlendEquationEXT ptr_vkCmdSetColorBlendEquationEXT;
PFN_vkCmdSetColorWriteMaskEXT ptr_vkCmdSetColorWriteMaskEXT;
PFN_vkCmdSetRasterizationStreamEXT ptr_vkCmdSetRasterizationStreamEXT;
PFN_vkCmdSetConservativeRasterizationModeEXT ptr_vkCmdSetConservativeRasterizationModeEXT;
PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT ptr_vkCmdSetExtraPrimitiveOverestimationSizeEXT;
PFN_vkCmdSetDepthClipEnableEXT ptr_vkCmdSetDepthClipEnableEXT;
PFN_vkCmdSetSampleLocationsEnableEXT ptr_vkCmdSetSampleLocationsEnableEXT;
PFN_vkCmdSetColorBlendAdvancedEXT ptr_vkCmdSetColorBlendAdvancedEXT;
PFN_vkCmdSetProvokingVertexModeEXT ptr_vkCmdSetProvokingVertexModeEXT;
PFN_vkCmdSetLineRasterizationModeEXT ptr_vkCmdSetLineRasterizationModeEXT;
PFN_vkCmdSetLineStippleEnableEXT ptr_vkCmdSetLineStippleEnableEXT;
PFN_vkCmdSetDepthClipNegativeOneToOneEXT ptr_vkCmdSetDepthClipNegativeOneToOneEXT;
PFN_vkCmdSetViewportWScalingEnableNV ptr_vkCmdSetViewportWScalingEnableNV;
PFN_vkCmdSetViewportSwizzleNV ptr_vkCmdSetViewportSwizzleNV;
PFN_vkCmdSetCoverageToColorEnableNV ptr_vkCmdSetCoverageToColorEnableNV;
PFN_vkCmdSetCoverageToColorLocationNV ptr_vkCmdSetCoverageToColorLocationNV;
PFN_vkCmdSetCoverageModulationModeNV ptr_vkCmdSetCoverageModulationModeNV;
PFN_vkCmdSetCoverageModulationTableEnableNV ptr_vkCmdSetCoverageModulationTableEnableNV;
PFN_vkCmdSetCoverageModulationTableNV ptr_vkCmdSetCoverageModulationTableNV;
PFN_vkCmdSetShadingRateImageEnableNV ptr_vkCmdSetShadingRateImageEnableNV;
PFN_vkCmdSetRepresentativeFragmentTestEnableNV ptr_vkCmdSetRepresentativeFragmentTestEnableNV;
PFN_vkCmdSetCoverageReductionModeNV ptr_vkCmdSetCoverageReductionModeNV;
#endif /* defined(VK_EXT_extended_dynamic_state3) */


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

#if defined(VK_EXT_extended_dynamic_state3)
    LOAD_DEVICE_FUNCTION(vkCmdSetTessellationDomainOriginEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthClampEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetPolygonModeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetRasterizationSamplesEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetSampleMaskEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetAlphaToCoverageEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetAlphaToOneEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetLogicOpEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetColorBlendEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetColorBlendEquationEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetColorWriteMaskEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetRasterizationStreamEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetConservativeRasterizationModeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetExtraPrimitiveOverestimationSizeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthClipEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetSampleLocationsEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetColorBlendAdvancedEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetProvokingVertexModeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetLineRasterizationModeEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetLineStippleEnableEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetDepthClipNegativeOneToOneEXT);
    LOAD_DEVICE_FUNCTION(vkCmdSetViewportWScalingEnableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetViewportSwizzleNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageToColorEnableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageToColorLocationNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageModulationModeNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageModulationTableEnableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageModulationTableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetShadingRateImageEnableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetRepresentativeFragmentTestEnableNV);
    LOAD_DEVICE_FUNCTION(vkCmdSetCoverageReductionModeNV);
#endif /* defined(VK_EXT_extended_dynamic_state3) */

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



void vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    ptr_vkCmdSetCullModeEXT(commandBuffer, cullMode);
}
void vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    ptr_vkCmdSetFrontFaceEXT(commandBuffer, frontFace);
}
void vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    ptr_vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
}
void vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    ptr_vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports);
}
void vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    ptr_vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors);
}
void vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    ptr_vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}
void vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    ptr_vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
}
void vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    ptr_vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
}
void vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    ptr_vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
}
void vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    ptr_vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
}
void vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    ptr_vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
}
void vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    ptr_vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}
#endif /* defined(VK_EXT_extended_dynamic_state) */



#if defined(VK_EXT_line_rasterization)
void vkCmdSetLineStippleEXT(VkCommandBuffer commandBuffer, uint32_t lineStippleFactor, uint16_t lineStipplePattern) {
    ptr_vkCmdSetLineStippleEXT(commandBuffer, lineStippleFactor, lineStipplePattern);
}
#endif /* defined(VK_EXT_line_rasterization) */



#if defined(VK_EXT_sample_locations)
void vkCmdSetSampleLocationsEXT(VkCommandBuffer commandBuffer, const VkSampleLocationsInfoEXT* pSampleLocationsInfo) {
    ptr_vkCmdSetSampleLocationsEXT(commandBuffer, pSampleLocationsInfo);
}
void vkGetPhysicalDeviceMultisamplePropertiesEXT(VkPhysicalDevice physicalDevice, VkSampleCountFlagBits samples, VkMultisamplePropertiesEXT* pMultisampleProperties) {
    ptr_vkGetPhysicalDeviceMultisamplePropertiesEXT(physicalDevice, samples, pMultisampleProperties);
}
#endif /* defined(VK_EXT_sample_locations) */



#if defined(VK_EXT_extended_dynamic_state2)
void vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    ptr_vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
}
void vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    ptr_vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
}
void vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    ptr_vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
}
void vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    ptr_vkCmdSetLogicOpEXT(commandBuffer, logicOp);
}
void vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    ptr_vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
}
#endif /* defined(VK_EXT_extended_dynamic_state2) */



#if defined(VK_EXT_extended_dynamic_state3)
void vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer, VkTessellationDomainOrigin domainOrigin) {
    ptr_vkCmdSetTessellationDomainOriginEXT(commandBuffer, domainOrigin);
}
void vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClampEnable) {
    ptr_vkCmdSetDepthClampEnableEXT(commandBuffer, depthClampEnable);
}
void vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer, VkPolygonMode polygonMode) {
    ptr_vkCmdSetPolygonModeEXT(commandBuffer, polygonMode);
}
void vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits rasterizationSamples) {
    ptr_vkCmdSetRasterizationSamplesEXT(commandBuffer, rasterizationSamples);
}
void vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer, VkSampleCountFlagBits samples, const VkSampleMask* pSampleMask) {
    ptr_vkCmdSetSampleMaskEXT(commandBuffer, samples, pSampleMask);
}
void vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToCoverageEnable) {
    ptr_vkCmdSetAlphaToCoverageEnableEXT(commandBuffer, alphaToCoverageEnable);
}
void vkCmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer, VkBool32 alphaToOneEnable) {
    ptr_vkCmdSetAlphaToOneEnableEXT(commandBuffer, alphaToOneEnable);
}
void vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer, VkBool32 logicOpEnable) {
    ptr_vkCmdSetLogicOpEnableEXT(commandBuffer, logicOpEnable);
}
void vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkBool32* pColorBlendEnables) {
    ptr_vkCmdSetColorBlendEnableEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEnables);
}
void vkCmdSetColorBlendEquationEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendEquationEXT* pColorBlendEquations) {
    ptr_vkCmdSetColorBlendEquationEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendEquations);
}
void vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorComponentFlags* pColorWriteMasks) {
    ptr_vkCmdSetColorWriteMaskEXT(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}
void vkCmdSetRasterizationStreamEXT(VkCommandBuffer commandBuffer, uint32_t rasterizationStream) {
    ptr_vkCmdSetRasterizationStreamEXT(commandBuffer, rasterizationStream);
}
void vkCmdSetConservativeRasterizationModeEXT(VkCommandBuffer commandBuffer, VkConservativeRasterizationModeEXT conservativeRasterizationMode) {
    ptr_vkCmdSetConservativeRasterizationModeEXT(commandBuffer, conservativeRasterizationMode);
}
void vkCmdSetExtraPrimitiveOverestimationSizeEXT(VkCommandBuffer commandBuffer, float extraPrimitiveOverestimationSize) {
    ptr_vkCmdSetExtraPrimitiveOverestimationSizeEXT(commandBuffer, extraPrimitiveOverestimationSize);
}
void vkCmdSetDepthClipEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthClipEnable) {
    ptr_vkCmdSetDepthClipEnableEXT(commandBuffer, depthClipEnable);
}
void vkCmdSetSampleLocationsEnableEXT(VkCommandBuffer commandBuffer, VkBool32 sampleLocationsEnable) {
    ptr_vkCmdSetSampleLocationsEnableEXT(commandBuffer, sampleLocationsEnable);
}
void vkCmdSetColorBlendAdvancedEXT(VkCommandBuffer commandBuffer, uint32_t firstAttachment, uint32_t attachmentCount, const VkColorBlendAdvancedEXT* pColorBlendAdvanced) {
    ptr_vkCmdSetColorBlendAdvancedEXT(commandBuffer, firstAttachment, attachmentCount, pColorBlendAdvanced);
}
void vkCmdSetProvokingVertexModeEXT(VkCommandBuffer commandBuffer, VkProvokingVertexModeEXT provokingVertexMode) {
    ptr_vkCmdSetProvokingVertexModeEXT(commandBuffer, provokingVertexMode);
}
void vkCmdSetLineRasterizationModeEXT(VkCommandBuffer commandBuffer, VkLineRasterizationModeEXT lineRasterizationMode) {
    ptr_vkCmdSetLineRasterizationModeEXT(commandBuffer, lineRasterizationMode);
}
void vkCmdSetLineStippleEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stippledLineEnable) {
    ptr_vkCmdSetLineStippleEnableEXT(commandBuffer, stippledLineEnable);
}
void vkCmdSetDepthClipNegativeOneToOneEXT(VkCommandBuffer commandBuffer, VkBool32 negativeOneToOne) {
    ptr_vkCmdSetDepthClipNegativeOneToOneEXT(commandBuffer, negativeOneToOne);
}
void vkCmdSetViewportWScalingEnableNV(VkCommandBuffer commandBuffer, VkBool32 viewportWScalingEnable) {
    ptr_vkCmdSetViewportWScalingEnableNV(commandBuffer, viewportWScalingEnable);
}
void vkCmdSetViewportSwizzleNV(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewportSwizzleNV* pViewportSwizzles) {
    ptr_vkCmdSetViewportSwizzleNV(commandBuffer, firstViewport, viewportCount, pViewportSwizzles);
}
void vkCmdSetCoverageToColorEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageToColorEnable) {
    ptr_vkCmdSetCoverageToColorEnableNV(commandBuffer, coverageToColorEnable);
}
void vkCmdSetCoverageToColorLocationNV(VkCommandBuffer commandBuffer, uint32_t coverageToColorLocation) {
    ptr_vkCmdSetCoverageToColorLocationNV(commandBuffer, coverageToColorLocation);
}
void vkCmdSetCoverageModulationModeNV(VkCommandBuffer commandBuffer, VkCoverageModulationModeNV coverageModulationMode) {
    ptr_vkCmdSetCoverageModulationModeNV(commandBuffer, coverageModulationMode);
}
void vkCmdSetCoverageModulationTableEnableNV(VkCommandBuffer commandBuffer, VkBool32 coverageModulationTableEnable) {
    ptr_vkCmdSetCoverageModulationTableEnableNV(commandBuffer, coverageModulationTableEnable);
}
void vkCmdSetCoverageModulationTableNV(VkCommandBuffer commandBuffer, uint32_t coverageModulationTableCount, const float* pCoverageModulationTable) {
    ptr_vkCmdSetCoverageModulationTableNV(commandBuffer, coverageModulationTableCount, pCoverageModulationTable);
}
void vkCmdSetShadingRateImageEnableNV(VkCommandBuffer commandBuffer, VkBool32 shadingRateImageEnable) {
    ptr_vkCmdSetShadingRateImageEnableNV(commandBuffer, shadingRateImageEnable);
}
void vkCmdSetRepresentativeFragmentTestEnableNV(VkCommandBuffer commandBuffer, VkBool32 representativeFragmentTestEnable) {
    ptr_vkCmdSetRepresentativeFragmentTestEnableNV(commandBuffer, representativeFragmentTestEnable);
}
void vkCmdSetCoverageReductionModeNV(VkCommandBuffer commandBuffer, VkCoverageReductionModeNV coverageReductionMode) {
    ptr_vkCmdSetCoverageReductionModeNV(commandBuffer, coverageReductionMode);
}
#endif /* defined(VK_EXT_extended_dynamic_state3) */



#if defined(VK_EXT_vertex_input_dynamic_state)
void vkCmdSetVertexInputEXT(VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount, const VkVertexInputBindingDescription2EXT* pVertexBindingDescriptions, uint32_t vertexAttributeDescriptionCount, const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions) {
    ptr_vkCmdSetVertexInputEXT(commandBuffer, vertexBindingDescriptionCount, pVertexBindingDescriptions, vertexAttributeDescriptionCount, pVertexAttributeDescriptions);
}
#endif /* defined(VK_EXT_vertex_input_dynamic_state) */



#if defined(VK_EXT_color_write_enable)
void vkCmdSetColorWriteEnableEXT(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkBool32* pColorWriteEnables) {
    ptr_vkCmdSetColorWriteEnableEXT(commandBuffer, attachmentCount, pColorWriteEnables);
}
#endif /* defined(VK_EXT_color_write_enable) */



#if defined(VK_EXT_debug_utils)
VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo) {
    return ptr_vkSetDebugUtilsObjectNameEXT(device, pNameInfo);
}
VkResult vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo) {
    return ptr_vkSetDebugUtilsObjectTagEXT(device, pTagInfo);
}
void vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) {
    ptr_vkQueueBeginDebugUtilsLabelEXT(queue, pLabelInfo);
}
void vkQueueEndDebugUtilsLabelEXT(VkQueue queue) {
    ptr_vkQueueEndDebugUtilsLabelEXT(queue);
}
void vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo) {
    ptr_vkQueueInsertDebugUtilsLabelEXT(queue, pLabelInfo);
}
void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) {
    ptr_vkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}
void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer) {
    ptr_vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}
void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo) {
    ptr_vkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}
VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
    return ptr_vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}
void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) {
    ptr_vkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}
void vkSubmitDebugUtilsMessageEXT(VkInstance instance, VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData) {
    ptr_vkSubmitDebugUtilsMessageEXT(instance, messageSeverity, messageTypes, pCallbackData);
}
#endif /* defined(VK_EXT_debug_utils) */



#endif