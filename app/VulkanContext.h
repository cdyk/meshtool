#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanResources.h"
#include "VulkanFrameManager.h"

#define CHECK_VULKAN(a) do { VkResult rv = (a); assert(rv == VK_SUCCESS && #a);} while(0)
#define CHECK_BOOL(a) do { bool rv = (a); assert(rv && #a);} while(0)

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

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

  PFN_vkCreateAccelerationStructureNVX vkCreateAccelerationStructureNVX = nullptr;
  PFN_vkDestroyAccelerationStructureNVX vkDestroyAccelerationStructureNVX = nullptr;
  PFN_vkGetAccelerationStructureMemoryRequirementsNVX vkGetAccelerationStructureMemoryRequirementsNVX = nullptr;
  PFN_vkGetAccelerationStructureScratchMemoryRequirementsNVX vkGetAccelerationStructureScratchMemoryRequirementsNVX = nullptr;
  PFN_vkBindAccelerationStructureMemoryNVX vkBindAccelerationStructureMemoryNVX = nullptr;
  PFN_vkCmdBuildAccelerationStructureNVX vkCmdBuildAccelerationStructureNVX = nullptr;
  PFN_vkCmdCopyAccelerationStructureNVX vkCmdCopyAccelerationStructureNVX = nullptr;
  PFN_vkCmdTraceRaysNVX vkCmdTraceRaysNVX = nullptr;
  PFN_vkCreateRaytracingPipelinesNVX vkCreateRaytracingPipelinesNVX = nullptr;
  PFN_vkGetRaytracingShaderHandlesNVX vkGetRaytracingShaderHandlesNVX = nullptr;
  PFN_vkGetAccelerationStructureHandleNVX vkGetAccelerationStructureHandleNVX = nullptr;
  PFN_vkCmdWriteAccelerationStructurePropertiesNVX vkCmdWriteAccelerationStructurePropertiesNVX = nullptr;
  PFN_vkCompileDeferredNVX vkCompileDeferredNVX = nullptr;


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

  bool nvxRaytracing = false;
private:
  bool debugLayer = true;
  VkDebugUtilsMessengerEXT debugCallbackHandle;
};
