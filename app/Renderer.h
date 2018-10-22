#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "VulkanContext.h"
#include "ResourceManager.h"
#include "RenderTextureManager.h"
#include "RenderMeshManager.h"

struct Vec4f;
struct Mat4f;
struct Mat3f;


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
  bool tangentSpaceCoordSys = true;
  Texturing texturing = Texturing::ColorGradient;
  
  Renderer(Logger logger, VulkanContext* vCtx, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h);
  ~Renderer();


  void startFrame();

  void drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMeshHandle renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP);
  
  RenderTextureManager* textureManager = nullptr;
  RenderMeshManager* meshManager = nullptr;

private:
  Logger logger;
  VulkanContext* vCtx = nullptr;

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
  unsigned renamingCurr = 0;

  void buildPipelines(RenderPassHandle pass);

};