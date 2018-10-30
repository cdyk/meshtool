#pragma once
#include "Common.h"
#include "VulkanResources.h"
#include "RenderMeshManager.h"

class VulkanManager;

class Raycaster
{
public:
  Raycaster(Logger logger, VulkanManager* vulkanManager);
  ~Raycaster();
  void init();


  void update(VkCommandBuffer cmdBuf, Vector<RenderMeshHandle>& meshes);

private:
  Logger logger;
  VulkanManager* vulkanManager = nullptr;

  ShaderHandle shader;
  PipelineHandle pipeline;

  struct MeshData {
    RenderMeshHandle src;
    AccelerationStructureHandle acc;
    uint32_t meshGen = 0u;
  };

  RenderBufferHandle bindingTable;

  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;
};