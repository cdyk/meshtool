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

class Renderer
{
public:
  enum struct Texturing {
    None,
    Checker,
    ColorGradient
  };

  bool outlines = false;
  bool solid = true;
  bool tangentSpaceCoordSys = false;
  Texturing texturing = Texturing::None;
  
  Renderer(Logger logger, App* app);
  ~Renderer();

  void init();

  void update(Vector<Mesh*>& meshes);

  void draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);
  
  RenderTextureManager* textureManager = nullptr;


private:
  Logger logger;
  App* app = nullptr;

  struct MeshData
  {
    Mesh* src = nullptr;
    uint32_t geometryGeneration = 0;
    uint32_t colorGeneration = 0;

    RenderBufferHandle vtx;
    RenderBufferHandle nrm;
    RenderBufferHandle tan;
    RenderBufferHandle bnm;
    RenderBufferHandle tex;
    RenderBufferHandle col;

    RenderBufferHandle indices;
    uint32_t triangleCount = 0;

    RenderBufferHandle lines;
    uint32_t lineCount = 0;


  };
  Vector<MeshData> meshData;
  Vector<MeshData> newMeshData;

  struct {
    RenderBufferHandle vtxCol;
    ShaderHandle shader;
    PipelineHandle pipeline;
  } coordSys;


  PipelineHandle vanillaPipeline;
  PipelineHandle texturedPipeline;
  PipelineHandle wireFrontFacePipeline;
  PipelineHandle wireBothFacesPipeline;
  PipelineHandle linePipeline;


  ShaderHandle vanillaShader;
  ShaderHandle flatShader;
  ShaderHandle texturedShader;

  uint32_t viewport[4];
  uint32_t frameCount;

  RenderTextureHandle checkerTex;
  RenderTextureHandle colorGradientTex;


  SamplerHandle texSampler;

  struct Rename {
    FenceHandle ready;
    DescriptorSetHandle objBufDescSet;
    DescriptorSetHandle objBufSamplerDescSet;
    RenderBufferHandle objectBuffer;
  };
  Vector<Rename> renaming;
  unsigned renameNext = 0;

  void buildPipelines(RenderPassHandle pass);

};