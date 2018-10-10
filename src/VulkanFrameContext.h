#pragma once
#include "Common.h"
#include "VulkanContext.h"

struct RenderFence : ResourceBase
{
  RenderFence(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkFence fence = VK_NULL_HANDLE;
};
typedef ResourceHandle<RenderFence> RenderFenceHandle;

struct SwapChain : public ResourceBase
{
  VkSwapchainKHR swapChain = VK_NULL_HANDLE;
};
typedef ResourceHandle<SwapChain> SwapChainHandle;


class VulkanFrameContext : public VulkanContext
{
public:
  VulkanFrameContext(Logger logger, uint32_t framesInFlight,
                     const char**instanceExts, uint32_t instanceExtCount,
                     int hasPresentationSupport(VkInstance, VkPhysicalDevice, uint32_t queueFamily));

  ~VulkanFrameContext();

  virtual void init() override;

  virtual void houseKeep() override;

  virtual VkSurfaceKHR createSurface() = 0;

  void resize(uint32_t w, uint32_t h);

  RenderFenceHandle createFence(bool signalled);

  SwapChainHandle createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo);

  uint32_t framesInFlight;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surfaceFormat;
  VkPresentModeKHR presentMode;

  struct FrameData
  {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore semaphore = VK_NULL_HANDLE;
  };
  Vector<FrameData> frameData;

  SwapChainHandle swapChain;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

private:

  ResourceManager<RenderFence> fenceResources;
  ResourceManager<SwapChain> swapChainResources;

  void destroyFence(RenderFence*);
  void destroySwapChain(SwapChain* swapChain);

  VkSurfaceFormatKHR chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace);
  VkPresentModeKHR choosePresentMode(Vector<VkPresentModeKHR>& requestedModes);

};
