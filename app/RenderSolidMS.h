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

class RenderSolidMS
{
public:
  enum struct Texturing {
    None,
    Checker,
    ColorGradient
  };

  Texturing texturing = Texturing::None;

  RenderSolidMS(Logger logger, App* app);
  ~RenderSolidMS();

  void init();

  void update(Vector<Mesh*>& meshes);

  void draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);

  RenderTextureManager* textureManager = nullptr;


private:
  Logger logger;
  App* app = nullptr;
  Texturing inDescSet = Texturing::None;

  struct MeshData
  {
    Mesh* src = nullptr;
    uint32_t geometryGeneration = 0;
    uint32_t colorGeneration = 0;

    RenderBufferHandle vtx;

    RenderBufferHandle meshletData;
    RenderBufferHandle meshlets;
    uint32_t meshletCount = 0;
  };
  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  PipelineHandle vanillaPipeline;
  PipelineHandle texturedPipeline;

  ShaderHandle taskShader;
  ShaderHandle meshShader;
  ShaderHandle solidShader;
  ShaderHandle texturedShader;

  uint32_t viewport[4];

  RenderTextureHandle checkerTex;
  RenderTextureHandle colorGradientTex;
  SamplerHandle texSampler;

  void buildPipelines(RenderPassHandle pass);

};