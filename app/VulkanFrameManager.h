#pragma once
#include "Common.h"
#include "VulkanResources.h"

class VulkanContext;

class VulkanFrameManager
{
public:
  VulkanFrameManager(VulkanContext* vCtx, Logger logger, uint32_t framesInFlight) : vCtx(vCtx), logger(logger), framesInFlight(framesInFlight) {}


  void init();

  void houseKeep();

  void startFrame();
  void presentFrame();

  void resize(uint32_t w, uint32_t h);

  void copyBuffer(RenderBufferHandle dst, RenderBufferHandle src, VkDeviceSize size);
  void transitionImageLayout(ImageHandle image, VkImageLayout layout);
  void copyBufferToImage(ImageHandle dst, RenderBufferHandle src, uint32_t w, uint32_t h);
  void submitGraphics(CommandBufferHandle cmdBuf, bool wait = false);

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surfaceFormat;
  VkPresentModeKHR presentMode;


  struct FrameData
  {
    CommandPoolHandle commandPool;
    CommandBufferHandle commandBuffer;
    FenceHandle fence;
    SemaphoreHandle imageAcquiredSemaphore;
    SemaphoreHandle renderCompleteSemaphore;
  };
  Vector<FrameData> frameData;

  FrameData& frame() { return frameData[frameIndex]; }

  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t frameIndex = 0;
  uint32_t framesInFlight = 0;
  FrameData& currentFrameData() { return frameData[frameIndex]; }

  SwapChainHandle swapChain;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

  uint32_t swapChainIndex = ~0u;
  Vector<VkImage> backBufferImages;
  Vector<ImageViewHandle> backBufferViews;

private:
  VulkanContext* vCtx = nullptr;
  Logger logger = nullptr;


  VkSurfaceFormatKHR chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace);
  VkPresentModeKHR choosePresentMode(Vector<VkPresentModeKHR>& requestedModes);

};
