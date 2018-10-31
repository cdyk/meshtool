#pragma once
#include "Common.h"
#include "VulkanResources.h"
#include "RenderMeshManager.h"
#include "LinAlg.h"

class VulkanManager;

class Raycaster
{
public:
  Raycaster(Logger logger, VulkanManager* vulkanManager);
  ~Raycaster();
  void init();


  void update(Vector<RenderMeshHandle>& meshes);

  void draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat4f& Pinv);

private:
  Logger logger;
  VulkanManager* vulkanManager = nullptr;

  VkPhysicalDeviceRaytracingPropertiesNVX rtProps;

  ShaderHandle shader;
  PipelineHandle pipeline;

  struct MeshData {
    RenderMeshHandle src;
    AccelerationStructureHandle acc;
    uint32_t meshGen = 0u;
  };
  AccelerationStructureHandle topLevel;
  RenderBufferHandle topLevelInstances;

  RenderBufferHandle bindingTable;

  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  bool first = true;
  AccelerationStructureHandle acc;
  AccelerationStructureHandle topAcc;


};