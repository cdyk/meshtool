#include "VulkanInfos.h"
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
  requestedPresentModes.pushBack(VK_PRESENT_MODE_FIFO_KHR);
  presentMode = choosePresentMode(requestedPresentModes);

  auto * res = vCtx->resources;

  frameData.resize(framesInFlight);
  for (auto & frame : frameData) {
    frame.commandPool = res->createCommandPool(vCtx->queueFamilyIndex);
    frame.commandBuffer = res->createPrimaryCommandBuffer(frame.commandPool);
    frame.imageAcquiredSemaphore = res->createSemaphore();
    frame.renderCompleteSemaphore = res->createSemaphore();
    frame.fence = res->createFence(true);
  }
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
  auto rv = vkDeviceWaitIdle(vCtx->device);
  assert(rv == VK_SUCCESS);

}


void VulkanFrameManager::houseKeep()
{
 
}

void VulkanFrameManager::copyBuffer(RenderBufferHandle dst, RenderBufferHandle src, VkDeviceSize size)
{
  auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &vCtx->infos->commandBuffer.singleShot);
  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(cmdBuf.resource->cmdBuf, src.resource->buffer, dst.resource->buffer, 1, &copyRegion);
  vkEndCommandBuffer(cmdBuf.resource->cmdBuf);
  submitGraphics(cmdBuf, true);
}

void VulkanFrameManager::transitionImageLayout(ImageHandle image, VkImageLayout layout)
{
  auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &vCtx->infos->commandBuffer.singleShot);

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
  vkCmdPipelineBarrier(cmdBuf.resource->cmdBuf,
                       srcStage, dstStage,
                       0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier
  );

  vkEndCommandBuffer(cmdBuf.resource->cmdBuf);
  submitGraphics(cmdBuf, true);
  image.resource->layout = layout;
}

void VulkanFrameManager::copyBufferToImage(ImageHandle dst, RenderBufferHandle src, uint32_t w, uint32_t h)
{
  auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(currentFrameData().commandPool);
  vkBeginCommandBuffer(cmdBuf.resource->cmdBuf, &vCtx->infos->commandBuffer.singleShot);

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



void VulkanFrameManager::submitGraphics(CommandBufferHandle cmdBuf, bool wait)
{
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf.resource->cmdBuf;
  vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
  if (wait) vkQueueWaitIdle(vCtx->queue);
}
