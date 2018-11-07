#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "VulkanContext.h"
#include "ResourceManager.h"
#include "RenderTextureManager.h"

struct Vec4f;
struct Mat4f;
struct Mat3f;

class App;
class RenderTextureManager;

class RenderNormals
{
public:
  RenderNormals(Logger logger, App* app);
  ~RenderNormals();

  void init();

  void update(Vector<Mesh*>& meshes);

  void draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);

private:
  Logger logger;
  App* app = nullptr;

  struct MeshData
  {
    Mesh* src = nullptr;
    uint32_t geometryGeneration = 0;
    uint32_t colorGeneration = 0;
    RenderBufferHandle vtxNrm;
    uint32_t vertexCount = 0;
  };
  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  ShaderHandle vertexShader;
  ShaderHandle fragmentShader;

  PipelineHandle pipeline;

  uint32_t viewport[4];
  uint32_t frameCount;

  struct Rename {
    FenceHandle ready;
    DescriptorSetHandle objBufDescSet;
    RenderBufferHandle objectBuffer;
  };
  Vector<Rename> renaming;
  unsigned renameNext = 0;

  void buildPipelines(RenderPassHandle pass);

};