#include "VulkanContext.h"
#include "VulkanFrameManager.h"

void VulkanFrameManager::init()
{
  surface = vCtx->surfaceManager->createSurface(vCtx);

  VkBool32 supportsPresent = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(vCtx->physicalDevice, vCtx->queueFamilyIndex, surface, &supportsPresent);
  assert(supportsPresent == VK_TRUE);

  Vector<VkFormat> requestedFormats;
  requestedFormats.pushBack(VK_FORMAT_B8G8R8A8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_R8G8B8A8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_B8G8R8_UNORM);
  requestedFormats.pushBack(VK_FORMAT_R8G8B8_UNORM);
  surfaceFormat = chooseFormat(requestedFormats, VK_COLORSPACE_SRGB_NONLINEAR_KHR);

  Vector<VkPresentModeKHR> requestedPresentModes;
  requestedPresentModes.pushBack(VK_PRESENT_MODE_IMMEDIATE_KHR);  // no vsync.
  presentMode = choosePresentMode(requestedPresentModes);

#ifndef _DEBUG
  // Not advertised but works on nv hardware
  presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
#endif

  auto * res = vCtx->resources;
  auto device = vCtx->device;

  Vector<VkDescriptorPoolSize> poolSizes =
  {
      { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
  };
  if (vCtx->nvRayTracing) {
    poolSizes.pushBack(VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1000 });
  }


  frameData.resize(framesInFlight);
  for (auto & frame : frameData) {
    frame.commandPool = res->createCommandPool(vCtx->queueFamilyIndex);

    VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmdAllocInfo.commandPool = frame.commandPool.resource->cmdPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    CHECK_VULKAN(vkAllocateCommandBuffers(vCtx->device, &cmdAllocInfo, &frame.commandBuffer));

    VkDescriptorPoolCreateInfo descPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descPoolInfo.maxSets = 1000 * poolSizes.size32();
    descPoolInfo.poolSizeCount = poolSizes.size32();
    descPoolInfo.pPoolSizes = poolSizes.data();
    frame.descriptorPool = res->createDescriptorPool(&descPoolInfo);

    frame.uniformPool.buffer = res->createUniformBuffer(16 * 1024 * 1024);  // 16mb

    frame.imageAcquiredSemaphore = res->createSemaphore();
    frame.renderCompleteSemaphore = res->createSemaphore();
    frame.fence = res->createFence(true);

    VkQueryPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolInfo.queryCount = 32;
    poolInfo.pipelineStatistics = 0;
    CHECK_VULKAN(vkCreateQueryPool(device, &poolInfo, nullptr, &frame.timerPool));
    frame.hasTimings = false;
  }
}

void* VulkanFrameManager::allocUniformStore(VkDescriptorBufferInfo& bufferInfo, size_t size)
{
  //VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment
  auto align = vCtx->physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
  assert(align && (align & (align - 1)) == 0 && "minUniformBufferOffsetAlignment is either zero or not a power of two.");

  auto & f = frame();
  auto offset = f.uniformPool.fill;
  f.uniformPool.fill = (f.uniformPool.fill + size + (align - 1u)) & ~(align - 1u);
  assert(f.uniformPool.fill < f.uniformPool.buffer.resource->alignedSize);

  bufferInfo.buffer = f.uniformPool.buffer.resource->buffer;
  bufferInfo.offset = offset;
  bufferInfo.range = f.uniformPool.fill - offset;
  return (char*)f.uniformPool.buffer.resource->hostPtr + offset;
}

VkDescriptorSet  VulkanFrameManager::allocDescriptorSet(PipelineHandle& pipe)
{
  auto & f = frame();
  auto device = vCtx->device;

  VkDescriptorSetAllocateInfo descAllocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  descAllocInfo.descriptorPool = f.descriptorPool.resource->pool;
  descAllocInfo.descriptorSetCount = 1;
  descAllocInfo.pSetLayouts = &pipe.resource->descLayout;

  VkDescriptorSet set = VK_NULL_HANDLE;
  CHECK_VULKAN(vkAllocateDescriptorSets(device, &descAllocInfo, &set));

  return set;
}

VkDescriptorSet VulkanFrameManager::allocVariableDescriptorSet(uint32_t* counts, uint32_t N, PipelineHandle& pipe)
{
  auto & f = frame();
  auto device = vCtx->device;

  // an array of descriptor counts, with each member specifying the number of descriptors in a
  // variable descriptor count binding in the corresponding descriptor set being allocated.

  VkDescriptorSetVariableDescriptorCountAllocateInfoEXT varDescInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
  varDescInfo.descriptorSetCount = N;
  varDescInfo.pDescriptorCounts = counts;

  VkDescriptorSetAllocateInfo descAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
  descAllocInfo.pNext = &varDescInfo;
  descAllocInfo.descriptorPool = f.descriptorPool.resource->pool;
  descAllocInfo.descriptorSetCount = 1;
  descAllocInfo.pSetLayouts = &pipe.resource->descLayout;


  VkDescriptorSet set = VK_NULL_HANDLE;
  CHECK_VULKAN(vkAllocateDescriptorSets(device, &descAllocInfo, &set));

  return set;
}



VkSurfaceFormatKHR VulkanFrameManager::chooseFormat(Vector<VkFormat>& requestedFormats, VkColorSpaceKHR requestedColorSpace)
{
  assert(requestedFormats.size() != 0);

  uint32_t count = 0;
  auto rv = vkGetPhysicalDeviceSurfaceFormatsKHR(vCtx->physicalDevice, surface, &count, nullptr);
  assert(rv == VK_SUCCESS);

  Vector<VkSurfaceFormatKHR> availableFormats(count);
  rv = vkGetPhysicalDeviceSurfaceFormatsKHR(vCtx->physicalDevice, surface, &count, availableFormats.data());
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

VkPresentModeKHR VulkanFrameManager::choosePresentMode(Vector<VkPresentModeKHR>& requestedModes)
{
  assert(requestedModes.size() != 0);

  uint32_t count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(vCtx->physicalDevice, surface, &count, nullptr);

  Vector<VkPresentModeKHR> availableModes(count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(vCtx->physicalDevice, surface, &count, availableModes.data());

  for (auto & requestedMode : requestedModes) {
    for (auto & availableMode : availableModes) {
      if (requestedMode == availableMode) return requestedMode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}




void VulkanFrameManager::resize(uint32_t w, uint32_t h)
{
  SwapChainHandle oldSwapChain = swapChain;
  CHECK_VULKAN(vkDeviceWaitIdle(vCtx->device));

  uint32_t minImageCount = 0;
  switch (presentMode) {
  case VK_PRESENT_MODE_MAILBOX_KHR: minImageCount = 3; break;
  case VK_PRESENT_MODE_FIFO_KHR: minImageCount = 2; break;
  case VK_PRESENT_MODE_FIFO_RELAXED_KHR: minImageCount = 2; break;
  case VK_PRESENT_MODE_IMMEDIATE_KHR: minImageCount = 1; break;
  default: assert(false && "Unhandled present mode");
  }

  VkSurfaceCapabilitiesKHR cap;
  CHECK_VULKAN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vCtx->physicalDevice, vCtx->frameManager->surface, &cap));
  if (minImageCount < cap.minImageCount) minImageCount = cap.minImageCount;
  if (cap.maxImageCount && cap.maxImageCount < minImageCount) minImageCount = cap.maxImageCount;

  if (cap.currentExtent.width == 0xffffffff) {
    width = w;
    height = h;
  }
  else {
    width = cap.currentExtent.width;
    height = cap.currentExtent.height;
  }

 
  VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
  swapChainInfo.surface = surface;
  swapChainInfo.minImageCount = minImageCount;
  swapChainInfo.imageFormat = surfaceFormat.format;
  swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapChainInfo.imageArrayLayers = 1;
  swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
  swapChainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapChainInfo.presentMode = presentMode;
  swapChainInfo.clipped = VK_TRUE;
  swapChainInfo.imageExtent.width = width;
  swapChainInfo.imageExtent.height = height;

  swapChain = vCtx->resources->createSwapChain(swapChain, swapChainInfo);

  uint32_t backBufferCount = 0;
  CHECK_VULKAN(vkGetSwapchainImagesKHR(vCtx->device, swapChain.resource->swapChain, &backBufferCount, NULL));
  backBufferImages.resize(backBufferCount);
  CHECK_VULKAN(vkGetSwapchainImagesKHR(vCtx->device, swapChain.resource->swapChain, &backBufferCount, backBufferImages.data()));

  backBufferViews.resize(backBufferCount);
  for (uint32_t i = 0; i < backBufferCount; i++) {
    backBufferViews[i] = vCtx->resources->createImageView(backBufferImages[i], surfaceFormat.format, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
  }
}

void VulkanFrameManager::startFrame()
{
  auto device = vCtx->device;

  frameIndex = frameIndex + 1;
  if (frameIndex < framesInFlight) frameIndex = 0;

  auto & f = frame();
  f.uniformPool.fill = 0;

  CHECK_VULKAN(vkAcquireNextImageKHR(vCtx->device,
                                     swapChain.resource->swapChain, UINT64_MAX,
                                     frame().imageAcquiredSemaphore.resource->semaphore, VK_NULL_HANDLE, &swapChainIndex));
  CHECK_VULKAN(vkWaitForFences(device, 1, &f.fence.resource->fence, VK_TRUE, UINT64_MAX));
  CHECK_VULKAN(vkResetFences(device, 1, &f.fence.resource->fence));
  CHECK_VULKAN(vkResetCommandPool(device, f.commandPool.resource->cmdPool, 0));
  CHECK_VULKAN(vkResetDescriptorPool(device, f.descriptorPool.resource->pool, 0));
}

void VulkanFrameManager::presentFrame()
{
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &frame().renderCompleteSemaphore.resource->semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &swapChain.resource->swapChain;
  info.pImageIndices = &swapChainIndex;
  CHECK_VULKAN(vkQueuePresentKHR(vCtx->queue, &info));
}


void VulkanFrameManager::houseKeep()
{
 
}

VkCommandBuffer VulkanFrameManager::createPrimaryCommandBuffer()
{
  VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
  VkCommandBufferAllocateInfo cmdAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
  cmdAllocInfo.commandPool = currentFrameData().commandPool.resource->cmdPool;
  cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmdAllocInfo.commandBufferCount = 1;
  CHECK_VULKAN(vkAllocateCommandBuffers(vCtx->device, &cmdAllocInfo, &cmdBuf));
  return cmdBuf;
}

void  VulkanFrameManager::stageAndCopyBuffer(RenderBufferHandle dst, const void* src, VkDeviceSize size)
{
  auto device = vCtx->device;

  auto staging = vCtx->resources->createStagingBuffer(size);
  assert(staging.resource->hostPtr);

  std::memcpy(staging.resource->hostPtr, src, size);
  copyBuffer(dst, staging, size);
}


void VulkanFrameManager::copyBuffer(RenderBufferHandle dst, RenderBufferHandle src, VkDeviceSize size)
{
  auto cmdBuf = createPrimaryCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(cmdBuf, src.resource->buffer, dst.resource->buffer, 1, &copyRegion);
  vkEndCommandBuffer(cmdBuf);
  submitGraphics(cmdBuf, true);
}

void VulkanFrameManager::transitionImageLayout(ImageHandle image, VkImageLayout layout)
{
  auto cmdBuf = createPrimaryCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = image.resource->layout;
  barrier.newLayout = layout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image.resource->image;
  if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    //barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcStage{};
  VkPipelineStageFlags dstStage{};
  if (image.resource->layout == VK_IMAGE_LAYOUT_UNDEFINED && layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (image.resource->layout == VK_IMAGE_LAYOUT_UNDEFINED && layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  else if (image.resource->layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;   // Move to argument?
  }
  else {
    assert(false && "unhandled transition");
  }
  vkCmdPipelineBarrier(cmdBuf, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

  vkEndCommandBuffer(cmdBuf);
  submitGraphics(cmdBuf, true);
  image.resource->layout = layout;
}

void VulkanFrameManager::copyBufferToImage(ImageHandle dst, RenderBufferHandle src, uint32_t w, uint32_t h)
{
  auto cmdBuf = createPrimaryCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { w, h, 1 };
  vkCmdCopyBufferToImage(cmdBuf, src.resource->buffer, dst.resource->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkEndCommandBuffer(cmdBuf);
  submitGraphics(cmdBuf, true);
}



void VulkanFrameManager::submitGraphics(VkCommandBuffer cmdBuf, bool wait)
{
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf;
  vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
  if (wait) vkQueueWaitIdle(vCtx->queue);
}

