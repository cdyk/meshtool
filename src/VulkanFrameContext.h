#pragma once
#include "Common.h"
#include "VulkanResourceContext.h"




class VulkanFrameContext : public VulkanResourceContext
{
public:
  VulkanFrameContext(Logger logger, uint32_t framesInFlight,
                     const char**instanceExts, uint32_t instanceExtCount);

  virtual ~VulkanFrameContext();

  virtual void init() override;

  virtual void houseKeep() override;

  virtual VkSurfaceKHR createSurface() = 0;

  void resize(uint32_t w, uint32_t h);

  void updateDescriptorSet(DescriptorSetHandle descriptorSet, RenderBufferHandle buffer);

  void copyBuffer(RenderBufferHandle dst, RenderBufferHandle src, VkDeviceSize size);
  void transitionImageLayout(RenderImageHandle image, VkImageLayout layout);
  void copyBufferToImage(RenderImageHandle dst, RenderBufferHandle src, uint32_t w, uint32_t h);
  void submitGraphics(CommandBufferHandle cmdBuf, bool wait = false);



  uint32_t framesInFlight;
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
  FrameData& currentFrameData() { return frameData[frameIndex]; }


  SwapChainHandle swapChain;
  VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

private:



  VkSurfaceFormatKHR chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace);
  VkPresentModeKHR choosePresentMode(Vector<VkPresentModeKHR>& requestedModes);

};
