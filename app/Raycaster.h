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

  VkDescriptorPool descPool;
  struct Rename {
    ImageHandle offscreenImage;
    ImageViewHandle offscreenView;

    VkDescriptorSet descSet;

  };
  Vector<Rename> renames;
  uint32_t renameIndex = 0;


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

  uint32_t w = ~0u;
  uint32_t h = ~0u;
  void resize(const Vec4f& viewport);

};