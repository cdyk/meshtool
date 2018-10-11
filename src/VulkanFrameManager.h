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

  void resize(uint32_t w, uint32_t h);

  void updateDescriptorSet(DescriptorSetHandle descriptorSet, RenderBufferHandle buffer);

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

  uint32_t frameIndex = 0;
  uint32_t framesInFlight = 0;
  FrameData& currentFrameData() { return frameData[frameIndex]; }


  SwapChainHandle swapChain;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

private:
  VulkanContext* vCtx = nullptr;
  Logger logger = nullptr;


  VkSurfaceFormatKHR chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace);
  VkPresentModeKHR choosePresentMode(Vector<VkPresentModeKHR>& requestedModes);

};
