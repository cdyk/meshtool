#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <cassert>
#include "VulkanManager.h"
#include "Common.h"
#include "VulkanContext.h"
#include "Renderer.h"
#include "LinAlgOps.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

namespace
{
  ImGui_ImplVulkanH_WindowData imguiWindowData;


  class GLFWPresentationSupport : public ISurfaceManager
  {
  public:
    GLFWPresentationSupport(GLFWwindow* window) : window(window) {}

    VkSurfaceKHR createSurface(VulkanContext* vCtx) override
    {
      VkSurfaceKHR surface;
      auto rv = glfwCreateWindowSurface(vCtx->instance, window, nullptr, &surface);
      assert(rv == VK_SUCCESS);
      return surface;

    }

    bool hasPresentationSupport(VulkanContext* vCtx, uint32_t queueFamily) override
    {
      return glfwGetPhysicalDevicePresentationSupport(vCtx->instance, vCtx->physicalDevice, queueFamily);
    }

  private:
    GLFWwindow* window;
  };

  void check_vk_result(VkResult err)
  {
    if (err == 0) return;
    fprintf(stderr, "VkResult %d", err);
    if (err < 0)
      abort();
  }

  bool initSwapChain(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
  {
    VkSurfaceKHR surface;
    auto rv = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    assert(rv == VK_SUCCESS);
    imguiWindowData.Surface = surface;

    VkBool32 supportsPresent = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &supportsPresent);
    assert(supportsPresent == VK_TRUE);

    uint32_t formatCount = 0;
    rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);
    assert(rv == VK_SUCCESS);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    assert(rv == VK_SUCCESS);

    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    imguiWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDevice, surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    imguiWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(physicalDevice, surface, present_modes, IM_ARRAYSIZE(present_modes));
    return true;
  }

  void uploadFonts(VkDevice device, VkQueue queue)
  {
    VkCommandPool command_pool = imguiWindowData.Frames[imguiWindowData.FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = imguiWindowData.Frames[imguiWindowData.FrameIndex].CommandBuffer;

    auto rv = vkResetCommandPool(device, command_pool, 0);
    assert(rv == VK_SUCCESS);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    rv = vkBeginCommandBuffer(command_buffer, &begin_info);
    assert(rv == VK_SUCCESS);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    rv = vkEndCommandBuffer(command_buffer);
    assert(rv == VK_SUCCESS);

    rv = vkQueueSubmit(queue, 1, &end_info, VK_NULL_HANDLE);
    assert(rv == VK_SUCCESS);

    rv = vkDeviceWaitIdle(device);
    assert(rv == VK_SUCCESS);

    ImGui_ImplVulkan_InvalidateFontUploadObjects();
  }

}

VulkanManager::VulkanManager(Logger l, GLFWwindow* window, uint32_t w, uint32_t h)
{
  logger = l;

  uint32_t extensions_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  vCtx = new VulkanContext(logger, extensions, extensions_count, IMGUI_VK_QUEUED_FRAMES, new GLFWPresentationSupport(window));
  vCtx->init();

  imguiWindowData.Surface = vCtx->frameManager->surface;
  imguiWindowData.SurfaceFormat = vCtx->frameManager->surfaceFormat;
  imguiWindowData.PresentMode = vCtx->frameManager->presentMode;
  for (int i = 0; i < IMGUI_VK_QUEUED_FRAMES; i++) {
    auto& fd = imguiWindowData.Frames[i];
    fd.CommandPool = vCtx->frameManager->frameData[i].commandPool.resource->cmdPool;
    fd.CommandBuffer = vCtx->frameManager->frameData[i].commandBuffer.resource->cmdBuf;
    fd.Fence = vCtx->frameManager->frameData[i].fence.resource->fence;
    fd.ImageAcquiredSemaphore = vCtx->frameManager->frameData[i].imageAcquiredSemaphore.resource->semaphore;
    fd.RenderCompleteSemaphore = vCtx->frameManager->frameData[i].renderCompleteSemaphore.resource->semaphore;
  }
  ImGui_ImplGlfw_InitForVulkan(window, true);


  resize(w, h);

  renderer = new Renderer(logger, vCtx, imguiWindowData.BackBufferView, imguiWindowData.BackBufferCount, w, h);


  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = vCtx->instance;
  init_info.PhysicalDevice = vCtx->physicalDevice;
  init_info.Device = vCtx->device;
  init_info.QueueFamily = vCtx->queueFamilyIndex;
  init_info.Queue = vCtx->queue;
  init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = vCtx->descPool;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, imguiWindowData.RenderPass);
  uploadFonts(vCtx->device, vCtx->queue);

}

VulkanManager::~VulkanManager()
{
  rendererPass.release();
  rendererFrameBuffers.resize(0);

  delete renderer;

  auto rv = vkDeviceWaitIdle(vCtx->device);
  assert(rv == VK_SUCCESS);

  ImGui_ImplVulkan_Shutdown();
  delete vCtx;
}


void VulkanManager::startFrame()
{
  vCtx->houseKeep();
  ImGui_ImplVulkan_NewFrame();

}

void VulkanManager::resize(uint32_t w, uint32_t h)
{
  auto * wd = &imguiWindowData;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  wd->ClearEnable = false;

  uint32_t min_image_count = 2;

  VkResult err;
  VkSwapchainKHR old_swapchain = wd->Swapchain;
  err = vkDeviceWaitIdle(vCtx->device);
  check_vk_result(err);

  wd->BackBufferCount = 0;

  // If min image count was not specified, request different count of images dependent on selected present mode
  if (min_image_count == 0)
    min_image_count = ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(wd->PresentMode);

  // Create Swapchain
  {
    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = wd->Surface;
    info.minImageCount = min_image_count;
    info.imageFormat = wd->SurfaceFormat.format;
    info.imageColorSpace = wd->SurfaceFormat.colorSpace;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
    info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = wd->PresentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = old_swapchain;
    VkSurfaceCapabilitiesKHR cap;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vCtx->physicalDevice, wd->Surface, &cap);
    check_vk_result(err);
    if (info.minImageCount < cap.minImageCount)
      info.minImageCount = cap.minImageCount;
    else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
      info.minImageCount = cap.maxImageCount;

    if (cap.currentExtent.width == 0xffffffff)
    {
      info.imageExtent.width = wd->Width = w;
      info.imageExtent.height = wd->Height = h;
    }
    else
    {
      info.imageExtent.width = wd->Width = cap.currentExtent.width;
      info.imageExtent.height = wd->Height = cap.currentExtent.height;
    }
    err = vkCreateSwapchainKHR(vCtx->device, &info, nullptr, &wd->Swapchain);
    check_vk_result(err);
    err = vkGetSwapchainImagesKHR(vCtx->device, wd->Swapchain, &wd->BackBufferCount, NULL);
    check_vk_result(err);
    err = vkGetSwapchainImagesKHR(vCtx->device, wd->Swapchain, &wd->BackBufferCount, wd->BackBuffer);
    check_vk_result(err);
  }
  if (old_swapchain)
    vkDestroySwapchainKHR(vCtx->device, old_swapchain, nullptr);


  {
    VkAttachmentDescription attachment = {};
    attachment.format = wd->SurfaceFormat.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = wd->ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment = {};
    color_attachment.attachment = 0;
    color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    imguiRenderPass = vCtx->resources->createRenderPass(&attachment, 1, &subpass, 1, &dependency);
    wd->RenderPass = imguiRenderPass.resource->pass;
  }

  {
    backBufferViews.resize(wd->BackBufferCount);
    for (uint32_t i = 0; i < wd->BackBufferCount; i++) {
      backBufferViews[i] = vCtx->resources->createImageView(wd->BackBuffer[i], wd->SurfaceFormat.format, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
      wd->BackBufferView[i] = backBufferViews[i].resource->view;
    }

    Vector<ImageViewHandle> attachments(1);
    imguiFrameBuffers.resize(wd->BackBufferCount);
    for (uint32_t i = 0; i < wd->BackBufferCount; i++) {
      attachments[0] = backBufferViews[i];
      imguiFrameBuffers[i] = vCtx->resources->createFrameBuffer(imguiRenderPass, w, h, attachments);
      wd->Framebuffer[i] = imguiFrameBuffers[i].resource->fb;
    }
  }


  {
    VkAttachmentDescription attachments[2];
    attachments[0].format = wd->SurfaceFormat.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags = 0;

    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags = 0;

    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depth_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;
    rendererPass = vCtx->resources->createRenderPass(attachments, 2, &subpass, 1, nullptr);
  }

  auto depthImage = vCtx->resources->createImage(w, h, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT);
  vCtx->frameManager->transitionImageLayout(depthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  Vector<ImageViewHandle> attachments(2);
  auto viewInfo = vCtx->infos->imageView.baseLevel2D;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  attachments[1] = vCtx->resources->createImageView(depthImage, viewInfo);

  rendererFrameBuffers.resize(wd->BackBufferCount);
  for (uint32_t i = 0; i < wd->BackBufferCount; i++) {
    attachments[0] = backBufferViews[i];
    rendererFrameBuffers[i] = vCtx->resources->createFrameBuffer(rendererPass, w, h, attachments);
  }

}


void VulkanManager::render(uint32_t w, uint32_t h, Vector<RenderMeshHandle>& renderMeshes, const Vec4f& viewerViewport, const Mat4f& P, const Mat4f& M)
{
  VkSemaphore& image_acquired_semaphore = imguiWindowData.Frames[imguiWindowData.FrameIndex].ImageAcquiredSemaphore;
  auto rv = vkAcquireNextImageKHR(vCtx->device, imguiWindowData.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &imguiWindowData.FrameIndex);
  assert(rv == VK_SUCCESS);

  auto * frameData = &imguiWindowData.Frames[imguiWindowData.FrameIndex];
  rv = vkWaitForFences(vCtx->device, 1, &frameData->Fence, VK_TRUE, UINT64_MAX);
  assert(rv == VK_SUCCESS);

  rv = vkResetFences(vCtx->device, 1, &frameData->Fence);
  assert(rv == VK_SUCCESS);

  rv = vkResetCommandPool(vCtx->device, frameData->CommandPool, 0);
  assert(rv == VK_SUCCESS);

  VkCommandBufferBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  rv = vkBeginCommandBuffer(frameData->CommandBuffer, &info);
  assert(rv == VK_SUCCESS);

  VkClearValue clearValues[2] = {};
  clearValues[1].depthStencil.depth = 1.f;

  {
    DebugScope debugScope(vCtx, frameData->CommandBuffer, "My render pass");

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = rendererPass.resource->pass;
    beginInfo.framebuffer = rendererFrameBuffers[imguiWindowData.FrameIndex].resource->fb;
    beginInfo.renderArea.extent.width = w;
    beginInfo.renderArea.extent.height = h;
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValues;


    Mat4f Z(1, 0, 0, 0,
            0, -1, 0, 0,
            0, 0, 0.5f, 0.5f,
            0, 0, 0.0, 1.f);

    auto MVP = mul(Z, mul(P, M));


    vkCmdBeginRenderPass(frameData->CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    for (size_t i = 0; i < renderMeshes.size(); i++) {
      renderer->drawRenderMesh(frameData->CommandBuffer, rendererPass, renderMeshes[i], viewerViewport, Mat3f(M), MVP);
    }
    vkCmdEndRenderPass(frameData->CommandBuffer);
  }

  VkRenderPassBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginInfo.renderPass = imguiWindowData.RenderPass;
  beginInfo.framebuffer = imguiWindowData.Framebuffer[imguiWindowData.FrameIndex];
  beginInfo.renderArea.extent.width = imguiWindowData.Width;
  beginInfo.renderArea.extent.height = imguiWindowData.Height;
  beginInfo.clearValueCount = 1;
  beginInfo.pClearValues = &imguiWindowData.ClearValue;

  vkCmdBeginRenderPass(frameData->CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameData->CommandBuffer);
  vkCmdEndRenderPass(frameData->CommandBuffer);


  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &image_acquired_semaphore;
  submitInfo.pWaitDstStageMask = &wait_stage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &frameData->CommandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &frameData->RenderCompleteSemaphore;

  rv = vkEndCommandBuffer(frameData->CommandBuffer);
  assert(rv == VK_SUCCESS);

  rv = vkQueueSubmit(vCtx->queue, 1, &submitInfo, frameData->Fence);
  assert(rv == VK_SUCCESS);
}

void VulkanManager::present()
{
  ImGui_ImplVulkanH_FrameData* fd = &imguiWindowData.Frames[imguiWindowData.FrameIndex];
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &fd->RenderCompleteSemaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &imguiWindowData.Swapchain;
  info.pImageIndices = &imguiWindowData.FrameIndex;
  auto rv = vkQueuePresentKHR(vCtx->queue, &info);
  assert(rv == VK_SUCCESS);
}
