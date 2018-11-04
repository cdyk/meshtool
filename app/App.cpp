#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <cassert>


#include "App.h"
#include "Viewer.h"
#include "Common.h"
#include "VulkanContext.h"
#include "Renderer.h"
#include "RenderOutlines.h"
#include "Raycaster.h"
#include "ImGuiRenderer.h"
#include "LinAlgOps.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace
{

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

}

App::App(Logger l, GLFWwindow* window, uint32_t w, uint32_t h) :
  window(window),
  width(w),
  height(h),
  fpsStart(glfwGetTime())
{
  viewer = new Viewer();
  logger = l;

  uint32_t extensions_count = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  vCtx = new VulkanContext(logger, extensions, extensions_count, 3, new GLFWPresentationSupport(window));
  vCtx->init();

  resize(w, h);

  renderer = new Renderer(logger, this);
  renderer->init();

  imGuiRenderer = new ImGuiRenderer(logger, this);
  imGuiRenderer->init();
}

App::~App()
{
  imGuiRenderer->shutdown();
  delete imGuiRenderer;

  delete renderer;

  rendererPass = RenderPassHandle();
  rendererFrameBuffers.resize(0);

  imguiRenderPass = RenderPassHandle();
  imguiFrameBuffers.resize(0);

  vCtx->shutdown();

  delete vCtx;
  delete viewer;
}


void App::startFrame()
{
  vCtx->houseKeep();
  imGuiRenderer->startFrame();
}

void App::resize(uint32_t w, uint32_t h)
{
  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  vCtx->frameManager->resize(w, h);

  {
    bool clear = false;
    VkAttachmentDescription attachment = {};
    attachment.format = vCtx->frameManager->surfaceFormat.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
  }

  {
    auto backBufferCount = vCtx->frameManager->backBufferViews.size32();
    auto * backBuffers = vCtx->frameManager->backBufferViews.data();

    Vector<ImageViewHandle> attachments(1);
    imguiFrameBuffers.resize(vCtx->frameManager->backBufferViews.size());
    for (uint32_t i = 0; i < vCtx->frameManager->backBufferViews.size(); i++) {
      attachments[0] = vCtx->frameManager->backBufferViews[i];
      imguiFrameBuffers[i] = vCtx->resources->createFrameBuffer(imguiRenderPass, w, h, attachments);
    }
  }


  {
    VkAttachmentDescription attachments[2];
    attachments[0].format = vCtx->frameManager->surfaceFormat.format;
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
  {
    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.format = VK_FORMAT_UNDEFINED;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.flags = 0;
    attachments[1] = vCtx->resources->createImageView(depthImage, viewInfo);
  }
  rendererFrameBuffers.resize(vCtx->frameManager->backBufferViews.size());
  for (uint32_t i = 0; i < vCtx->frameManager->backBufferViews.size(); i++) {
    attachments[0] = vCtx->frameManager->backBufferViews[i];
    rendererFrameBuffers[i] = vCtx->resources->createFrameBuffer(rendererPass, w, h, attachments);
  }

}


void App::render(const Vec4f& viewport)
{
  auto device = vCtx->device;

  auto cpuStart = glfwGetTime();

  const Mat4f& P = viewer->getProjectionMatrix();
  const Mat4f& M = viewer->getViewMatrix();
  const Mat4f& PMinv = viewer->getProjectionViewInverseMatrix();
  const Mat4f& Minv = viewer->getViewInverseMatrix();


  auto * frameMgr = vCtx->frameManager;
  frameMgr->startFrame();

  raytrace = raytrace && vCtx->nvxRaytracing;

  bool reallyLines = lines;
  for (auto * mesh : items.meshes) {
    reallyLines = mesh->lineCount && reallyLines;
  }

  if (reallyLines || outlines) {
    if (!renderOutlines) {
      renderOutlines = new RenderOutlines(logger, this);
      renderOutlines->init();
    }
    renderOutlines->update(items.meshes);
  }
  else if (renderOutlines) {
    delete renderOutlines;
    renderOutlines = nullptr;
  }


  if (raytrace) {
    if (raycaster == nullptr) {
      raycaster = new Raycaster(logger, this);
      raycaster->init();
    }
    raycaster->update(items.meshes);
  }
  else if (raycaster) {
    delete raycaster;
    raycaster = nullptr;
  }

  if (raytrace == false) {
    renderer->update(items.meshes);
  }

  auto & frame = vCtx->frameManager->frame();
  if (frame.hasTimings) {
    uint64_t results[2];
    vkGetQueryPoolResults(device, frame.timerPool, 0, 2, sizeof(results), results, sizeof(results[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    gpuTimeAcc += vCtx->physicalDeviceProperties.limits.timestampPeriod*(results[1] - results[0]);
    gpuTimeAccN++;
  }

  auto & cmdBuf = frame.commandBuffer;


  VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  CHECK_VULKAN(vkBeginCommandBuffer(cmdBuf, &info));
  vkCmdResetQueryPool(cmdBuf, frame.timerPool, 0, 2);
  vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timerPool, 0);


  VkClearValue clearValues[2] = {};
  clearValues[1].depthStencil.depth = 1.f;

  auto PM = mul(P, M);
  {
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = rendererPass.resource->pass;
    beginInfo.framebuffer = rendererFrameBuffers[frameMgr->swapChainIndex].resource->fb;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValues;


    Mat4f Z(1, 0, 0, 0,
            0, -1, 0, 0,
            0, 0, 0.5f, 0.5f,
            0, 0, 0.0, 1.f);

    auto MVP = mul(Z, mul(P, M));


    vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    if (raytrace == false) {
      renderer->draw(cmdBuf, rendererPass, viewport, Mat3f(M), MVP);
    }
    if (renderOutlines) {
      renderOutlines->draw(cmdBuf, rendererPass, viewport, Mat3f(M), MVP, outlines, lines);
    }
    vkCmdEndRenderPass(cmdBuf);
  }

  VkRenderPassBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginInfo.renderPass = imguiRenderPass.resource->pass;
  beginInfo.framebuffer = imguiFrameBuffers[frameMgr->swapChainIndex].resource->fb;
  beginInfo.renderArea.extent.width = frameMgr->width;
  beginInfo.renderArea.extent.height = frameMgr->height;
  beginInfo.clearValueCount = 1;
  beginInfo.pClearValues = clearValues;

  vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
  imGuiRenderer->recordRendering(frame.commandBuffer);

  vkCmdEndRenderPass(cmdBuf);

  if (raytrace) {
    raycaster->draw(cmdBuf, viewport, Mat3f(Minv), PMinv);
  }


  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &vCtx->frameManager->frame().imageAcquiredSemaphore.resource->semaphore;
  submitInfo.pWaitDstStageMask = &wait_stage;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &frame.renderCompleteSemaphore.resource->semaphore;


  vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timerPool, 1);
  frame.hasTimings = true;
  CHECK_VULKAN(vkEndCommandBuffer(cmdBuf));
  CHECK_VULKAN(vkQueueSubmit(vCtx->queue, 1, &submitInfo, frame.fence.resource->fence));

  auto cpuStop = glfwGetTime();
  cpuTimeAcc += (cpuStop - cpuStart);
  cpuTimeAccN++;

  if (10 < cpuTimeAccN || 0.5 < (cpuTimeAcc / cpuTimeAccN)) {
    auto fps = cpuTimeAccN / (glfwGetTime() - fpsStart);
    fpsStart = glfwGetTime();

    snprintf(fpsString, sizeof(fpsString), "FPS %.2f CPU %.2fms GPU %.2fms",
             fps,
             1000.f*(cpuTimeAcc / cpuTimeAccN),
             1e-6f*(gpuTimeAcc / gpuTimeAccN));

    cpuTimeAcc = 0.0;
    gpuTimeAcc = 0.0;
    cpuTimeAccN = 0;
    gpuTimeAccN = 0;
  }
}

void App::present()
{
  vCtx->frameManager->presentFrame();
}
