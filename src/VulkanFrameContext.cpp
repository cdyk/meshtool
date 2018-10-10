#include "VulkanFrameContext.h"


VulkanFrameContext::VulkanFrameContext(Logger logger, uint32_t framesInFlight,
                                       const char**instanceExts, uint32_t instanceExtCount,
                                       int hasPresentationSupport(VkInstance, VkPhysicalDevice, uint32_t queueFamily)) :
  VulkanContext(logger, instanceExts, instanceExtCount, hasPresentationSupport),
  framesInFlight(framesInFlight)
{
}

VulkanFrameContext::~VulkanFrameContext()
{
  logger(0, "%d unreleased fences", fenceResources.getCount());
}


void VulkanFrameContext::init()
{
  VulkanContext::init();

  surface = createSurface();

  VkBool32 supportsPresent = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &supportsPresent);
  assert(supportsPresent == VK_TRUE);

  Vector<VkFormat> requestedFormats;
  requestedFormats.pushBack(VK_FORMAT_B8G8R8A8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_R8G8B8A8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_B8G8R8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_R8G8B8_UNORM);
  surfaceFormat = chooseFormat(requestedFormats, VK_COLORSPACE_SRGB_NONLINEAR_KHR);

  Vector<VkPresentModeKHR> requestedPresentModes;
  requestedPresentModes.pushBack(VK_PRESENT_MODE_FIFO_KHR);
  presentMode = choosePresentMode(requestedPresentModes);

#if 0

  M_ASSERT(physical_device != VK_NULL_HANDLE && device != VK_NULL_HANDLE);
  (void)allocator;

  // Create Command Buffers
  VkResult err;
  for (int i = 0; i < IMGUI_VK_QUEUED_FRAMES; i++)
  {
    ImGui_ImplVulkanH_FrameData* fd = &wd->Frames[i];
    {
      VkCommandPoolCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      info.queueFamilyIndex = queue_family;
      err = vkCreateCommandPool(device, &info, allocator, &fd->CommandPool);
      check_vk_result(err);
    }
    {
      VkCommandBufferAllocateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      info.commandPool = fd->CommandPool;
      info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      info.commandBufferCount = 1;
      err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
      check_vk_result(err);
    }
    {
      VkFenceCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      err = vkCreateFence(device, &info, allocator, &fd->Fence);
      check_vk_result(err);
    }
    {
      VkSemaphoreCreateInfo info = {};
      info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      err = vkCreateSemaphore(device, &info, allocator, &fd->ImageAcquiredSemaphore);
      check_vk_result(err);
      err = vkCreateSemaphore(device, &info, allocator, &fd->RenderCompleteSemaphore);
      check_vk_result(err);
    }
  }
#endif
}

VkSurfaceFormatKHR VulkanFrameContext::chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace)
{
  assert(requestedFormats.size() != 0);

  uint32_t count = 0;
  auto rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, nullptr);
  assert(rv == VK_SUCCESS);

  Vector<VkSurfaceFormatKHR> availableFormats(count);
  rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &count, availableFormats.data());
  assert(rv == VK_SUCCESS);

  if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
    VkSurfaceFormatKHR surfaceFormat{};
    surfaceFormat.format = requestedFormats[0];
    surfaceFormat.colorSpace = requestedColorSpace;
    return surfaceFormat;
  }
  for (auto & requestedFormat : requestedFormats) {
    for (auto & availableFormat : availableFormats) {
      if (availableFormat.format == requestedFormat && availableFormat.colorSpace == requestedColorSpace) {
        return availableFormat;
      }
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR VulkanFrameContext::choosePresentMode(Vector<VkPresentModeKHR>& requestedModes)
{
  assert(requestedModes.size() != 0);

  uint32_t count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, nullptr);

  Vector<VkPresentModeKHR> availableModes(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, availableModes.data());

  for (auto & requestedMode : requestedModes) {
    for (auto & availableMode : availableModes) {
      if (requestedMode == availableMode) return requestedMode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}




void VulkanFrameContext::resize(uint32_t w, uint32_t h)
{
  auto rv = vkDeviceWaitIdle(device);
  assert(rv == VK_SUCCESS);

}


void VulkanFrameContext::houseKeep()
{
  VulkanContext::houseKeep();

  {
    Vector<RenderFence*> orphans;
    fenceResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyFence(r);
      delete r;
    }
  }
  {
    Vector<SwapChain*> orphans;
    swapChainResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroySwapChain(r);
      delete r;
    }
  }


}

RenderFenceHandle VulkanFrameContext::createFence(bool signalled)
{
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

  auto fenceHandle = fenceResources.createResource();
  auto rv = vkCreateFence(device, &info, nullptr, &fenceHandle.resource->fence);
  assert(rv == VK_SUCCESS);
  return fenceHandle;
}

void VulkanFrameContext::destroyFence(RenderFence* fence)
{
  if (fence->fence) vkDestroyFence(device, fence->fence, nullptr);
  fence->fence = nullptr;
}



SwapChainHandle VulkanFrameContext::createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo)
{
  return SwapChainHandle();
}


void VulkanFrameContext::destroySwapChain(SwapChain* swapChain)
{

}
