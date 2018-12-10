#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanResources.h"
#include "VulkanFrameManager.h"

#define CHECK_VULKAN(a) do { VkResult rv = (a); assert(rv == VK_SUCCESS && #a);} while(0)
#define CHECK_BOOL(a) do { bool rv = (a); assert(rv && #a);} while(0)



class ISurfaceManager
{
public:
  virtual VkSurfaceKHR createSurface(VulkanContext* vCtx) = 0;
  virtual bool hasPresentationSupport(VulkanContext* vCtx, uint32_t queueFamily) = 0;
};



class VulkanTextureManager;

class VulkanContext
{
public:
  VulkanContext() = delete;

  VulkanContext(Logger logger, const char**instanceExts, uint32_t instanceExtCount, uint32_t framesInFlight, ISurfaceManager* presentationSupport);
  ~VulkanContext();

  void init();
  void houseKeep();
  void shutdown();

  VkInstance instance = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  VkPipelineCache pipelineCache = VK_NULL_HANDLE;

  PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV = nullptr;
  PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV = nullptr;
  PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV = nullptr;
  //PFN_vkGetAccelerationStructureScratchMemoryRequirementsNV vkGetAccelerationStructureScratchMemoryRequirementsNV = nullptr;
  PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV = nullptr;
  PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV = nullptr;
  PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV = nullptr;
  PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV = nullptr;
  PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV = nullptr;
  PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV = nullptr;
  PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV = nullptr;
  //PFN_vkCmdWriteAccelerationStructurePropertiesNV vkCmdWriteAccelerationStructurePropertiesNV = nullptr;
  PFN_vkCompileDeferredNV vkCompileDeferredNV = nullptr;

  PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV = nullptr;
  PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV = nullptr;
  PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV = nullptr;

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;

  Logger logger = nullptr;
  ISurfaceManager* surfaceManager = nullptr;
  VulkanResources* resources = nullptr;
  VulkanFrameManager* frameManager = nullptr;
  VulkanTextureManager* textureManager = nullptr;

  bool nvRayTracing = false;
  bool nvMeshShader = false;
private:
#ifdef _DEBUG
  bool debugLayer = true;
#else
  bool debugLayer = false;
#endif
  VkDebugUtilsMessengerEXT debugCallbackHandle;
};
