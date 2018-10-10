#pragma once
#include <list>
#include "Common.h"
#include "VulkanResources.h"
#include "Renderer.h"

struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class VulkanManager
{
public:
  VulkanManager(Logger l, GLFWwindow* window, uint32_t w, uint32_t h);
  ~VulkanManager();

  void resize(uint32_t w, uint32_t h);

  void startFrame();

  void render(uint32_t w, uint32_t h, Vector<RenderMeshHandle>& renderMeshes, const Vec4f& viewerViewport, const Mat4f& P, const Mat4f& M);

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

