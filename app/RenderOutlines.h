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

class RenderOutlines
{
public:
  RenderOutlines(Logger logger, App* app);
  ~RenderOutlines();

  void init();

  void update(Vector<Mesh*>& meshes);

  void draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP, bool outlines, bool lines);

private:
  Logger logger;
  App* app = nullptr;

  struct MeshData
  {
    Mesh* src = nullptr;
    uint32_t geometryGeneration = 0;
    uint32_t colorGeneration = 0;

    RenderBufferHandle vtx;
    RenderBufferHandle col;

    RenderBufferHandle indices;
    uint32_t vertexCount = 0;
    uint32_t outlineCount = 0;
    uint32_t lineOffset = 0;
    uint32_t lineCount = 0;
  };
  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  ShaderHandle vertexShader;
  ShaderHandle fragmentShader;


  PipelineHandle linePipeline;



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