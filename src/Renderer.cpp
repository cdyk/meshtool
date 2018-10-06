#include <cassert>
#include "Common.h"
#include "Renderer.h"
#include "Mesh.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"

struct ObjectBuffer
{
  Mat4f MVP;
  Mat3f N;
};


struct RenderMesh
{
  RenderBufferHandle vtxNrmTex;
  uint32_t tri_n = 0;
};

namespace {

  uint32_t flatVS[]
#include "flatVS.glsl.h"
    ;

  uint32_t vanillaVS[]
#include "vanillaVS.glsl.h"
    ;

  uint32_t vanillaPS[]
#include "vanillaPS.glsl.h"
    ;


  void vanillaPipelineInfo(VulkanContext* vCtx,
                           Vector<VkVertexInputBindingDescription>& inputBind,
                           Vector<VkVertexInputAttributeDescription>& inputAttrib,
                           VkPipelineLayoutCreateInfo& pipeLayoutCI,
                           VkDescriptorSetLayoutBinding& descSetLayoutBind,
                           VkDescriptorSetLayoutCreateInfo& descLayoutCI)
  {
    {
      inputBind.resize(1);
      inputBind[0] = vCtx->infos.vertexInput.v32b;

      inputAttrib.resize(3);
      inputAttrib[0] = vCtx->infos.vertexInput.v3f_0_0b;
      inputAttrib[1] = vCtx->infos.vertexInput.v3f_1_12b;
      inputAttrib[2] = vCtx->infos.vertexInput.v2f_2_24b;
    }
    {
      descSetLayoutBind = {};
      descSetLayoutBind.binding = 0;
      descSetLayoutBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descSetLayoutBind.descriptorCount = 1;
      descSetLayoutBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
      descSetLayoutBind.pImmutableSamplers = NULL;

      descLayoutCI = {};
      descLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      descLayoutCI.pNext = NULL;
      descLayoutCI.bindingCount = 1;
      descLayoutCI.pBindings = &descSetLayoutBind;
    }
    {
      pipeLayoutCI = {};
      pipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipeLayoutCI.pNext = NULL;
      pipeLayoutCI.pushConstantRangeCount = 0;
      pipeLayoutCI.pPushConstantRanges = NULL;
    }
  }

  void wirePipelineInfo(VulkanContext* vCtx, 
                        Vector<VkVertexInputBindingDescription>& inputBind,
                        Vector<VkVertexInputAttributeDescription>& inputAttrib,
                        VkPipelineLayoutCreateInfo& pipeLayoutCI,
                        VkDescriptorSetLayoutBinding& descSetLayoutBind,
                        VkDescriptorSetLayoutCreateInfo& descLayoutCI)
  {
    {
      inputBind.resize(1);
      inputBind[0] = vCtx->infos.vertexInput.v32b;

      inputAttrib.resize(1);
      inputAttrib[0] = vCtx->infos.vertexInput.v3f_0_0b;
    }
    {
      descSetLayoutBind = {};
      descSetLayoutBind.binding = 0;
      descSetLayoutBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descSetLayoutBind.descriptorCount = 1;
      descSetLayoutBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
      descSetLayoutBind.pImmutableSamplers = NULL;

      descLayoutCI = {};
      descLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      descLayoutCI.pNext = NULL;
      descLayoutCI.bindingCount = 1;
      descLayoutCI.pBindings = &descSetLayoutBind;
    }
    {
      pipeLayoutCI = {};
      pipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      pipeLayoutCI.pNext = NULL;
      pipeLayoutCI.pushConstantRangeCount = 0;
      pipeLayoutCI.pPushConstantRanges = NULL;
    }
  }


}


Renderer::Renderer(Logger logger, VulkanContext* vCtx, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h) :
  logger(logger),
  vCtx(vCtx)
{

  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { vanillaPS, sizeof(vanillaPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    vanillaShader = vCtx->createShader(stages);
  }
  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { flatVS, sizeof(vanillaVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { vanillaPS, sizeof(vanillaPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    flatShader = vCtx->createShader(stages);
  }

  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->createFence(true);
    renaming[i].objectBuffer = vCtx->createUniformBuffer(sizeof(ObjectBuffer));
  }

}

Renderer::~Renderer()
{
}

RenderMesh* Renderer::createRenderMesh(Mesh* mesh)
{
  struct VtxNrmTex
  {
    Vec3f vtx;
    Vec3f nrm;
    Vec2f tex;
  };

  auto * renderMesh = new RenderMesh();
  renderMesh->tri_n = mesh->tri_n;
  
  renderMesh->vtxNrmTex = vCtx->createVertexBuffer(sizeof(VtxNrmTex) * 3 * renderMesh->tri_n);

  {
    MappedBuffer<VtxNrmTex> map(vCtx, renderMesh->vtxNrmTex);
    for (unsigned i = 0; i < mesh->tri_n; i++) {
      Vec3f p[3];
      for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->tri[3 * i + k]];
      auto n = cross(p[1] - p[0], p[2] - p[0]);
      for (unsigned k = 0; k < 3; k++) {
        map.mem[3 * i + k].vtx = p[k];
        map.mem[3 * i + k].nrm = n;
        map.mem[3 * i + k].tex = Vec2f(0.f);
      }
    }
  }

  logger(0, "CreateRenderMesh");
  return renderMesh;
}


void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  if(!vanillaPipeline || vanillaPipeline.resource->pass != pass)
  {
    Vector<VkVertexInputBindingDescription> inputBind;
    Vector<VkVertexInputAttributeDescription> inputAttrib;
    VkPipelineLayoutCreateInfo pipeLayoutCI;
    VkDescriptorSetLayoutBinding descSetLayoutBind;
    VkDescriptorSetLayoutCreateInfo descLayoutCI;
    vanillaPipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI, descSetLayoutBind, descLayoutCI);

    vanillaPipeline = vCtx->createPipeline(inputBind,
                                           inputAttrib,
                                           pipeLayoutCI,
                                           descLayoutCI,
                                           pass,
                                           vanillaShader,
                                           vCtx->infos.pipelineRasterization.cullBackDepthBias);

    wirePipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI, descSetLayoutBind, descLayoutCI);
    wirePipeline = vCtx->createPipeline(inputBind,
                                        inputAttrib,
                                        pipeLayoutCI,
                                        descLayoutCI,
                                        pass,
                                        flatShader,
                                        vCtx->infos.pipelineRasterization.cullBackLine);
    for (size_t i = 0; i < renaming.size(); i++) {
      renaming[i].vanillaDescSet = vCtx->createDescriptorSet(vanillaPipeline.resource->descLayout);
      renaming[i].wireDescSet = vCtx->createDescriptorSet(wirePipeline.resource->descLayout);
    }
  }
  auto & rename = renaming[renamingCurr];

  {
    MappedBuffer<ObjectBuffer> map(vCtx, rename.objectBuffer);
    map.mem->MVP = MVP;
    map.mem->N = N;
  }
  vCtx->updateDescriptorSet(rename.vanillaDescSet, rename.objectBuffer);
  vCtx->updateDescriptorSet(rename.wireDescSet, rename.objectBuffer);

  {
    VkViewport vp = {};
    vp.x = viewport.x;
    vp.y = viewport.y;
    vp.width = viewport.z;
    vp.height = viewport.w;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuf, 0, 1, &vp);
  }
  {
    VkRect2D scissor;
    scissor.offset.x = int32_t(viewport.x);
    scissor.offset.y = int32_t(viewport.y);
    scissor.extent.width = uint32_t(viewport.z);
    scissor.extent.height = uint32_t(viewport.w);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
  }

  const VkDeviceSize offsets[1] = { 0 };
  vkCmdBindVertexBuffers(cmdBuf, 0, 1, &renderMesh->vtxNrmTex.resource->buffer, offsets);

  {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);

    VkDescriptorSet desc_set[1] = { rename.vanillaDescSet.resource->descSet };
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);
    vkCmdDraw(cmdBuf, 3 * renderMesh->tri_n, 1, 0, 0);
  }

  {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, wirePipeline.resource->pipe);

    VkDescriptorSet desc_set[1] = { rename.wireDescSet.resource->descSet };
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);
    vkCmdDraw(cmdBuf, 3 * renderMesh->tri_n, 1, 0, 0);
  }


  renamingCurr = (renamingCurr + 1);
  if (renaming.size() <= renamingCurr) renamingCurr = 0;
}

void Renderer::destroyRenderMesh(RenderMesh* renderMesh)
{
  logger(0, "destroyRenderMesh");
  delete renderMesh;
}

