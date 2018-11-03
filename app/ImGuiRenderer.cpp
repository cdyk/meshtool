#include "ImGuiRenderer.h"
#include "App.h"
#include "VulkanContext.h"

#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

namespace
{

  void check_vk_result(VkResult err)
  {
    if (err == 0) return;
    fprintf(stderr, "VkResult %d", err);
    if (err < 0)
      abort();
  }

}


void ImGuiRenderer::init()
{
  auto * vCtx = app->vCtx;
  auto & frame = vCtx->frameManager->frame();
  auto cmdPool = frame.commandPool.resource->cmdPool;
  auto cmdBuf = frame.commandBuffer;

  ImGui_ImplGlfw_InitForVulkan(app->window, true);

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
  ImGui_ImplVulkan_Init(&init_info, app->imguiRenderPass.resource->pass);

  CHECK_VULKAN(vkResetCommandPool(vCtx->device, cmdPool, 0));

  VkCommandBufferBeginInfo begin_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CHECK_VULKAN(vkBeginCommandBuffer(cmdBuf, &begin_info));

  ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);

  VkSubmitInfo end_info = {};
  end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  end_info.commandBufferCount = 1;
  end_info.pCommandBuffers = &cmdBuf;

  CHECK_VULKAN(vkEndCommandBuffer(cmdBuf));
  CHECK_VULKAN(vkQueueSubmit(vCtx->queue, 1, &end_info, VK_NULL_HANDLE));
  CHECK_VULKAN(vkDeviceWaitIdle(vCtx->device));

  ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

void ImGuiRenderer::startFrame()
{
  ImGui_ImplVulkan_NewFrame();
}

void ImGuiRenderer::recordRendering(VkCommandBuffer commandBuffer)
{
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiRenderer::shutdown()
{
  ImGui_ImplVulkan_Shutdown();
}
