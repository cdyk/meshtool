#pragma once
#include "Common.h"
#include "VulkanResources.h"
#include "RenderMeshManager.h"
#include "LinAlg.h"

class App;

class Raycaster
{
public:
  Raycaster(Logger logger, App* app);
  ~Raycaster();
  void init();


  void update(Vector<RenderMeshHandle>& meshes);

  void draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat3f& Ninv, const Mat4f& Pinv);

private:
  Logger logger;
  App* app = nullptr;

  VkPhysicalDeviceRaytracingPropertiesNVX rtProps;

  ShaderHandle shader;
  PipelineHandle pipeline;

  VkDescriptorPool descPool = VK_NULL_HANDLE;
  struct Rename {
    RenderBufferHandle sceneBuffer;

    ImageHandle offscreenImage;
    ImageViewHandle offscreenView;

    VkDescriptorSet descSet = VK_NULL_HANDLE;

  };
  Vector<Rename> renames;
  uint32_t renameIndex = 0;
  uint32_t rndState = 42;
  uint32_t stationaryFrames = 0;
  Mat4f prevPinv;

  struct MeshData {
    RenderBufferHandle vertices;
    RenderBufferHandle indices;
    RenderBufferHandle triangleData;
    RenderMeshHandle src;
    AccelerationStructureHandle acc;
    VkGeometryNVX geometry;

    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
    uint32_t meshGen = 0u;
    bool rebuild;
  };
  AccelerationStructureHandle topLevel;
  RenderBufferHandle topLevelInstances;

  RenderBufferHandle bindingTable;

  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  AccelerationStructureHandle topAcc;

  uint32_t w = ~0u;
  uint32_t h = ~0u;

  void resize(const Vec4f& viewport);
  bool updateMeshData(VkDeviceSize& scratchSize, MeshData& meshData, const RenderMeshHandle& renderMesh);

  void buildPipeline();
  void buildDescriptorSets();

  VkDescriptorSetLayout buildDescriptorSetLayout();

};