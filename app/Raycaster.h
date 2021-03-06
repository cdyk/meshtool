#pragma once
#include "Common.h"
#include "VulkanResources.h"
#include "Mesh.h"
#include "LinAlg.h"

class App;

class Raycaster
{
public:
  Raycaster(Logger logger, App* app);
  ~Raycaster();
  void init();


  void update(Vector<Mesh*>& meshes);

  void draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat3f& Ninv, const Mat4f& Pinv);

private:
  Logger logger;
  App* app = nullptr;

  VkPhysicalDeviceRayTracingPropertiesNV rtProps;

  ShaderHandle rgenShader;
  ShaderHandle chitShader;
  ShaderHandle missShader;
  PipelineHandle pipeline;

  ImageHandle offscreenImage[2];
  ImageViewHandle offscreenView[2];
  uint32_t offscreenIndex = 0;

  uint32_t rndState = 42;
  uint32_t stationaryFrames = 0;
  Mat4f prevPinv;

  struct MeshData {
    Mesh* src = nullptr;
    uint32_t geometryGeneration = 0;
    uint32_t colorGeneration = 0;

    RenderBufferHandle vertices;
    RenderBufferHandle indices;
    RenderBufferHandle triangleData;
    AccelerationStructureHandle acc;
    VkGeometryNV geometry;

    uint32_t vertexCount = 0;
    uint32_t triangleCount = 0;
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
  bool updateMeshData(VkDeviceSize& scratchSize, MeshData& meshData, const Mesh* mesh);

  void buildPipeline();

  VkDescriptorSetLayout buildDescriptorSetLayout();

  VkDescriptorSet setupDescriptorSet(const Mat3f& Ninv, const Mat4f& Pinv, VkImageView outImage, VkImageView inImage);
  

};