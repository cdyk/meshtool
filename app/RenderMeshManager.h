#pragma once
#include "RenderResource.h"
#include "VulkanResources.h"

struct RenderMeshResource : RenderResourceBase
{
  RenderMeshResource(ResourceManagerBase& manager) : RenderResourceBase(manager) {}
  Mesh* mesh = nullptr;

  // indexed geometry
  RenderBufferHandle vertices;
  uint32_t vertexCount = 0;

  RenderBufferHandle indices;

  // unindexed geometry
  RenderBufferHandle vtx;
  RenderBufferHandle nrm;
  RenderBufferHandle tan;
  RenderBufferHandle bnm;
  RenderBufferHandle tex;
  RenderBufferHandle col;
  uint32_t tri_n = 0;

  RenderBufferHandle lines;
  uint32_t lineCount = 0;

  uint32_t generation = 1;
};
typedef ResourceHandle<RenderMeshResource> RenderMeshHandle;

class VulkanContext;

class RenderMeshManager : ResourceManager<RenderMeshResource>
{
public:
  RenderMeshManager(VulkanContext* vCtx, Logger logger);

  RenderMeshHandle createRenderMesh(Mesh* mesh);

  void updateRenderMeshColor(RenderMeshHandle renderMesh);

  void startFrame();

private:
  VulkanContext* vCtx = nullptr;
  Logger logger = nullptr;
};