#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "VulkanContext.h"
#include "ResourceManager.h"

struct RenderMesh : ResourceBase
{
  RenderMesh(ResourceManagerBase& manager) : ResourceBase(manager) {}
  Mesh* mesh = nullptr;
  RenderBufferHandle vtxNrmTex;
  RenderBufferHandle color;
  uint32_t tri_n = 0;
};
typedef ResourceHandle<RenderMesh> RenderMeshHandle;

struct Vec4f;
struct Mat4f;
struct Mat3f;

class Renderer
{
public:
  bool outlines = true;
  bool solid = true;

  Renderer(Logger logger, VulkanContext* vCtx, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h);
  ~Renderer();

  RenderMeshHandle createRenderMesh(Mesh* mesh);

  void houseKeep();

  void updateRenderMeshColor(RenderMeshHandle renderMesh);

  void drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMeshHandle renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);

private:
  Logger logger;
  VulkanContext* vCtx = nullptr;

  ResourceManager<RenderMesh> renderMeshResources;

  PipelineHandle vanillaPipeline;
  PipelineHandle wireFrontFacePipeline;
  PipelineHandle wireBothFacesPipeline;

  ShaderHandle vanillaShader;
  ShaderHandle flatShader;

  uint32_t viewport[4];
  uint32_t frameCount;

  ImageHandle texImage;
  ImageViewHandle texImageView;
  SamplerHandle texSampler;

  struct Rename {
    FenceHandle ready;
    DescriptorSetHandle sharedDescSet;
    RenderBufferHandle objectBuffer;
  };
  Vector<Rename> renaming;
  unsigned renamingCurr = 0;
};