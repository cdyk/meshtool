#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "VulkanFrameContext.h"


struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class Renderer
{
public:
  bool outlines = true;
  bool solid = true;

  Renderer(Logger logger, VulkanFrameContext* vCtx, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h);
  ~Renderer();

  RenderMesh* createRenderMesh(Mesh* mesh);

  void updateRenderMeshColor(RenderMesh* renderMesh);

  void drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);
  void destroyRenderMesh(RenderMesh* renderMesh);

private:
  Logger logger;
  VulkanFrameContext* vCtx = nullptr;

  PipelineHandle vanillaPipeline;
  PipelineHandle wireFrontFacePipeline;
  PipelineHandle wireBothFacesPipeline;

  ShaderHandle vanillaShader;
  ShaderHandle flatShader;

  uint32_t viewport[4];
  uint32_t frameCount;

  RenderImageHandle texImage;

  struct Rename {
    FenceHandle ready;
    DescriptorSetHandle sharedDescSet;
    RenderBufferHandle objectBuffer;
  };
  Vector<Rename> renaming;
  unsigned renamingCurr = 0;
};