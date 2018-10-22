#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanInfos.h"
#include "VulkanResources.h"
#include "VulkanFrameManager.h"

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

  VkInstance instance = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;

  Logger logger = nullptr;
  ISurfaceManager* surfaceManager = nullptr;
  VulkanInfos* infos = nullptr;
  VulkanResources* resources = nullptr;
  VulkanFrameManager* frameManager = nullptr;
  VulkanTextureManager* textureManager = nullptr;

private:
  bool debugLayer = true;
  VkDebugUtilsMessengerEXT debugCallbackHandle;
};
