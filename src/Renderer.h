#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "VulkanContext.h"


struct RenderMesh;
struct Vec4f;
struct Mat4f;

class Renderer
{
public:
  Renderer(Logger logger, VulkanContext* vCtx, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h);
  ~Renderer();

  RenderMesh* createRenderMesh(Mesh* mesh);

  void drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Vec4f& viewport, const Mat4f& MVP);
  void destroyRenderMesh(RenderMesh* renderMesh);

private:
  Logger logger;
  VulkanContext* vCtx = nullptr;

  PipelineHandle vanillaPipeline;
  ShaderHandle vanillaShader;
  uint32_t viewport[4];
  uint32_t frameCount;


  struct Rename {
    RenderFenceHandle ready;
    DescriptorSetHandle vanillaDescSet;
    RenderBufferHandle objectBuffer;
  };
  Vector<Rename> renaming;
  unsigned renamingCurr = 0;
};