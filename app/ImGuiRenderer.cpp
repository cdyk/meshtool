#include "ImGuiRenderer.h"
#include "VulkanManager.h"
#include "VulkanContext.h"

#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

namespace
{
  ImGui_ImplVulkanH_WindowData imguiWindowData;

  void check_vk_result(VkResult err)
  {
    if (err == 0) return;
    fprintf(stderr, "VkResult %d", err);
    if (err < 0)
      abort();
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


void ImGuiRenderer::init()
{
  auto * vCtx = vulkanManager->vCtx;

  ImGui_ImplGlfw_InitForVulkan(vulkanManager->window, true);

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
  ImGui_ImplVulkan_Init(&init_info, vulkanManager->imguiRenderPass.resource->pass);
  uploadFonts(vCtx->device, vCtx->queue);
}

