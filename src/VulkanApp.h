#pragma once
#include <list>
#include "Common.h"
#include "VulkanResources.h"

struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class VulkanApp
{
public:
  VulkanApp(Logger l, GLFWwindow* window, uint32_t w, uint32_t h);
  ~VulkanApp();

  class Renderer* main_VulkanInit();

  void resize(uint32_t w, uint32_t h);

  void startFrame();

  void render(uint32_t w, uint32_t h, Vector<RenderMesh*>& renderMeshes, const Vec4f& viewerViewport, const Mat4f& P, const Mat4f& M);

  void present();

  Logger logger;
  Renderer* renderer = nullptr;
  VulkanContext* vCtx = nullptr;

  RenderPassHandle rendererPass;
  RenderPassHandle imguiRenderPass;

  Vector<RenderImageHandle> backBufferViews;

  Vector<FrameBufferHandle> rendererFrameBuffers;
  Vector<FrameBufferHandle> imguiFrameBuffers;
};

