#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanInfos.h"

class VulkanContext
{
public:
  VulkanContext() = delete;

  VulkanContext(Logger logger, const char**instanceExts, uint32_t instanceExtCount);
  virtual ~VulkanContext();

  virtual void init();
  virtual void houseKeep();
  virtual bool hasPresentationSupport(uint32_t queueFamily) = 0;

  VulkanInfos infos;
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  //VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;

protected:
  Logger logger = nullptr;

private:
  bool debugLayer = true;
  VkDebugUtilsMessengerEXT debugCallbackHandle;
};
