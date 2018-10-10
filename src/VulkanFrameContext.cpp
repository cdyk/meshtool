#include "VulkanFrameContext.h"


VulkanFrameContext::VulkanFrameContext(Logger logger, uint32_t framesInFlight,
                                       const char**instanceExts, uint32_t instanceExtCount) :
  VulkanResourceContext(logger, instanceExts, instanceExtCount),
  framesInFlight(framesInFlight)
{
}

VulkanFrameContext::~VulkanFrameContext()
{
}


void VulkanFrameContext::init()
{
  VulkanResourceContext::init();

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

  frameData.resize(framesInFlight);
  for (auto & frame : frameData) {
    frame.commandPool = createCommandPool(queueFamilyIndex);
    frame.commandBuffer = createPrimaryCommandBuffer(frame.commandPool);
    frame.imageAcquiredSemaphore = createSemaphore();
    frame.renderCompleteSemaphore = createSemaphore();
    frame.fence = createFence(true);
  }
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
  VulkanResourceContext::houseKeep();
  
}

void VulkanFrameContext::copyBuffer(RenderBufferHandle dst, RenderBufferHandle src, VkDeviceSize size)
{
  auto cmdBuf = createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &infos.commandBuffer.singleShot);
  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(cmdBuf.resource->cmdBuf, src.resource->buffer, dst.resource->buffer, 1, &copyRegion);
  vkEndCommandBuffer(cmdBuf.resource->cmdBuf);
  submitGraphics(cmdBuf, true);
}

void VulkanFrameContext::transitionImageLayout(RenderImageHandle image, VkImageLayout layout)
{
  auto cmdBuf = createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &infos.commandBuffer.singleShot);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = image.resource->layout;
  barrier.newLayout = layout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image.resource->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = 0;

  vkCmdPipelineBarrier(cmdBuf.resource->cmdBuf,
                       0, 0,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier
  );

  vkEndCommandBuffer(cmdBuf.resource->cmdBuf);
  submitGraphics(cmdBuf, true);
  image.resource->layout = layout;
}

void VulkanFrameContext::copyBufferToImage(RenderImageHandle dst, RenderBufferHandle src, uint32_t w, uint32_t h)
{
  auto cmdBuf = createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &infos.commandBuffer.singleShot);

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
  vkCmdCopyBufferToImage(cmdBuf.resource->cmdBuf, src.resource->buffer, dst.resource->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  vkEndCommandBuffer(cmdBuf.resource->cmdBuf);
  submitGraphics(cmdBuf, true);
}



void VulkanFrameContext::submitGraphics(CommandBufferHandle cmdBuf, bool wait)
{
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf.resource->cmdBuf;
  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  if (wait) vkQueueWaitIdle(queue);
}


void VulkanFrameContext::updateDescriptorSet(DescriptorSetHandle descriptorSet, RenderBufferHandle buffer)
{
  VkDescriptorBufferInfo descBufInfo;
  descBufInfo.buffer = buffer.resource->buffer;
  descBufInfo.offset = 0;
  descBufInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[1];
  writes[0] = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = nullptr;
  writes[0].dstSet = descriptorSet.resource->descSet;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &descBufInfo;
  writes[0].dstArrayElement = 0;
  writes[0].dstBinding = 0;
  vkUpdateDescriptorSets(device, 1, writes, 0, NULL);
}