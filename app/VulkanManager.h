#pragma once
#include <list>
#include "Common.h"
#include "VulkanResources.h"
#include "RenderMeshManager.h"

struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class Renderer;
class ImGuiRenderer;

struct GLFWwindow;

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
  ImGuiRenderer* imGuiRenderer = nullptr;
  VulkanContext* vCtx = nullptr;

  RenderPassHandle rendererPass;
  RenderPassHandle imguiRenderPass;

  Vector<ImageViewHandle> backBufferViews;

  Vector<FrameBufferHandle> rendererFrameBuffers;
  Vector<FrameBufferHandle> imguiFrameBuffers;
};

