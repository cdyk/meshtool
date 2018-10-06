#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <cassert>
#include "VulkanMain.h"
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
  Logger logger;
  Renderer* renderer = nullptr;
  VulkanContext* vCtx = nullptr;
  ImGui_ImplVulkanH_WindowData imguiWindowData;
  RenderPassHandle mainPass;
  Vector<FrameBufferHandle> frameBuffers;

  void check_vk_result(VkResult err)
  {
    if (err == 0) return;
    logger(2, "VkResult %d", err);
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

Renderer* main_VulkanInit(Logger l, GLFWwindow* window, uint32_t w, uint32_t h)
{
  logger = l;

  uint32_t extensions_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  vCtx = new VulkanContext(logger, extensions, extensions_count, glfwGetPhysicalDevicePresentationSupport);
  initSwapChain(window, vCtx->instance, vCtx->physicalDevice, vCtx->queueFamilyIndex);

  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkanH_CreateWindowDataCommandBuffers(vCtx->physicalDevice, vCtx->device, vCtx->queueFamilyIndex, &imguiWindowData, nullptr);

  main_VulkanResize(w, h);

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

  return renderer;
}


void main_VulkanStartFrame()
{
  vCtx->houseKeep();
  ImGui_ImplVulkan_NewFrame();

}

void main_VulkanResize(uint32_t w, uint32_t h)
{
  auto * wd = &imguiWindowData;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  wd->ClearEnable = false;
  ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(vCtx->physicalDevice, vCtx->device, wd, nullptr, w, h);

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
    mainPass = vCtx->createRenderPass(attachments, 2, &subpass, 1);
  }

  Vector<RenderImageHandle> attachments(2);
  attachments[1] = vCtx->createRenderImage(w, h, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_D32_SFLOAT);

  frameBuffers.resize(wd->BackBufferCount);
  for (uint32_t i = 0; i < wd->BackBufferCount; i++) {
    attachments[0] = vCtx->wrapRenderImageView(wd->BackBufferView[i]);
    frameBuffers[i] = vCtx->createFrameBuffer(mainPass, w, h, attachments);
  }

}


void main_VulkanRender(uint32_t w, uint32_t h, Vector<RenderMesh*>& renderMeshes, const Vec4f& viewerViewport, const Mat4f& P, const Mat4f& M)
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
    beginInfo.renderPass = mainPass.resource->pass;
    beginInfo.framebuffer = frameBuffers[imguiWindowData.FrameIndex].resource->fb;
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
      renderer->drawRenderMesh(frameData->CommandBuffer, mainPass, renderMeshes[i], viewerViewport, Mat3f(M), MVP);
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

void main_VulkanPresent()
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


void main_VulkanCleanup()
{

  mainPass.release();
  frameBuffers.resize(0);

  delete renderer;

  auto rv = vkDeviceWaitIdle(vCtx->device);
  assert(rv == VK_SUCCESS);

  delete vCtx;
  ImGui_ImplVulkan_Shutdown();
}
