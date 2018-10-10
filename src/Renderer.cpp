#include <cassert>
#include "Common.h"
#include "Renderer.h"
#include "Mesh.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"

struct ObjectBuffer
{
  Mat4f MVP;
  Vec4f Ncol0;
  Vec4f Ncol1;
  Vec4f Ncol2;
};


struct RenderMesh
{
  Mesh* mesh = nullptr;
  RenderBufferHandle vtxNrmTex;
  RenderBufferHandle color;
  uint32_t tri_n = 0;
};

namespace {

  uint32_t flatVS[]
#include "flatVS.glsl.h"
    ;

  uint32_t flatPS[]
#include "flatPS.glsl.h"
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
                           VkPipelineLayoutCreateInfo& pipeLayoutCI)
  {
    {
      inputBind.resize(2);
      inputBind[0] = vCtx->infos.vertexInput.v32b;
      inputBind[0].binding = 0;
      inputBind[1] = vCtx->infos.vertexInput.v4b;
      inputBind[1].binding = 1;

      inputAttrib.resize(4);
      inputAttrib[0] = vCtx->infos.vertexInput.v3f_0_0b;
      inputAttrib[0].location = 0;
      inputAttrib[1] = vCtx->infos.vertexInput.v3f_1_12b;
      inputAttrib[1].location = 1;
      inputAttrib[2] = vCtx->infos.vertexInput.v2f_2_24b;
      inputAttrib[2].location = 2;
      inputAttrib[3] = vCtx->infos.vertexInput.v4u8_0b;
      inputAttrib[3].location = 3;
      inputAttrib[3].binding = 1;
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
                        VkPipelineLayoutCreateInfo& pipeLayoutCI)
  {
    {
      inputBind.resize(2);
      inputBind[0] = vCtx->infos.vertexInput.v32b;
      inputBind[0].binding = 0;
      inputBind[1] = vCtx->infos.vertexInput.v4b;
      inputBind[1].binding = 1;

      inputAttrib.resize(2);
      inputAttrib[0] = vCtx->infos.vertexInput.v3f_0_0b;
      inputAttrib[0].location = 0;
      inputAttrib[1] = vCtx->infos.vertexInput.v4u8_0b;
      inputAttrib[1].location = 1;
      inputAttrib[1].binding = 1;
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
    stages[0] = { flatVS, sizeof(flatVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { flatPS, sizeof(flatPS), VK_SHADER_STAGE_FRAGMENT_BIT };
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
  renderMesh->mesh = mesh;
  renderMesh->tri_n = mesh->triCount;
  
  renderMesh->vtxNrmTex = vCtx->createVertexBuffer(sizeof(VtxNrmTex) * 3 * renderMesh->tri_n);

  {
    MappedBuffer<VtxNrmTex> map(vCtx, renderMesh->vtxNrmTex);
    if (mesh->nrmCount) {
      for (unsigned i = 0; i < 3*mesh->triCount; i++) {
        map.mem[i].vtx = mesh->vtx[mesh->triVtxIx[i]];
        map.mem[i].nrm = mesh->nrm[mesh->triNrmIx[i]];
        map.mem[i].tex = Vec2f(0.f);
      }
    }
    else {
      for (unsigned i = 0; i < mesh->triCount; i++) {
        Vec3f p[3];
        for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
        auto n = cross(p[1] - p[0], p[2] - p[0]);
        for (unsigned k = 0; k < 3; k++) {
          map.mem[3 * i + k].vtx = p[k];
          map.mem[3 * i + k].nrm = n;
          map.mem[3 * i + k].tex = Vec2f(0.f);
        }
      }
    }
  }

  renderMesh->color = vCtx->createVertexBuffer(sizeof(uint32_t) * 3 * renderMesh->tri_n);
  updateRenderMeshColor(renderMesh);

  logger(0, "CreateRenderMesh");
  return renderMesh;
}

void Renderer::updateRenderMeshColor(RenderMesh* renderMesh)
{
  {
    MappedBuffer<uint32_t> map(vCtx, renderMesh->color);
    for (unsigned i = 0; i < renderMesh->mesh->triCount; i++) {
      auto color = renderMesh->mesh->currentColor[i];
      map.mem[3 * i + 0] = color;
      map.mem[3 * i + 1] = color;
      map.mem[3 * i + 2] = color;
    }
  }
  logger(0, "Updated mesh color");
}


void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  if(!vanillaPipeline || vanillaPipeline.resource->pass != pass)
  {
    VkDescriptorSetLayoutBinding descSetLayoutBind{};
    descSetLayoutBind = {};
    descSetLayoutBind.binding = 0;
    descSetLayoutBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetLayoutBind.descriptorCount = 1;
    descSetLayoutBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descSetLayoutBind.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo descLayoutCI{};
    descLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutCI.pNext = NULL;
    descLayoutCI.bindingCount = 1;
    descLayoutCI.pBindings = &descSetLayoutBind;

    Vector<VkVertexInputBindingDescription> inputBind;
    Vector<VkVertexInputAttributeDescription> inputAttrib;
    VkPipelineLayoutCreateInfo pipeLayoutCI;
    vanillaPipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI);

    vanillaPipeline = vCtx->createPipeline(inputBind,
                                           inputAttrib,
                                           pipeLayoutCI,
                                           descLayoutCI,
                                           pass,
                                           vanillaShader,
                                           vCtx->infos.pipelineRasterization.cullBackDepthBias);

    wirePipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI);
    wireFrontFacePipeline = vCtx->createPipeline(inputBind,
                                                 inputAttrib,
                                                 pipeLayoutCI,
                                                 descLayoutCI,
                                                 pass,
                                                 flatShader,
                                                 vCtx->infos.pipelineRasterization.cullBackLine);

    wireBothFacesPipeline = vCtx->createPipeline(inputBind,
                                                 inputAttrib,
                                                 pipeLayoutCI,
                                                 descLayoutCI,
                                                 pass,
                                                 flatShader,
                                                 vCtx->infos.pipelineRasterization.cullNoneLine);


    for (size_t i = 0; i < renaming.size(); i++) {
      renaming[i].sharedDescSet = vCtx->createDescriptorSet(vanillaPipeline.resource->descLayout);
    }
  }
  auto & rename = renaming[renamingCurr];

  {
    MappedBuffer<ObjectBuffer> map(vCtx, rename.objectBuffer);
    map.mem->MVP = MVP;
    map.mem->Ncol0 = Vec4f(N.cols[0], 0.f);
    map.mem->Ncol1 = Vec4f(N.cols[1], 0.f);
    map.mem->Ncol2 = Vec4f(N.cols[2], 0.f);
  }
  vCtx->updateDescriptorSet(rename.sharedDescSet, rename.objectBuffer);

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

  {
    VkBuffer buffers[2] = { renderMesh->vtxNrmTex.resource->buffer, renderMesh->color.resource->buffer };
    VkDeviceSize offsets[2] = { 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 2, buffers, offsets);
  }

  VkDescriptorSet desc_set[1] = { rename.sharedDescSet.resource->descSet };
  if(solid) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);
    vkCmdDraw(cmdBuf, 3 * renderMesh->tri_n, 1, 0, 0);
  }

  if(outlines) {
    if (solid) {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, wireFrontFacePipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);
    }
    else {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, wireBothFacesPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);
    }
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

