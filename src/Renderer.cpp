#include <cassert>
#include "Common.h"
#include "Renderer.h"
#include "Mesh.h"
#include "VulkanContext.h"

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

  uint32_t vanillaVS[]
#include "vanillaVS.glsl.h"
    ;

  uint32_t vanillaPS[]
#include "vanillaPS.glsl.h"
    ;


  void vanillaPipelineInfo(Vector<VkVertexInputBindingDescription>& inputBind,
                           Vector<VkVertexInputAttributeDescription>& inputAttrib,
                           VkPipelineLayoutCreateInfo& pipeLayoutCI,
                           VkDescriptorSetLayoutBinding& descSetLayoutBind,
                           VkDescriptorSetLayoutCreateInfo& descLayoutCI)
  {
    {
      inputBind.resize(1);
      inputBind[0].binding = 0;
      inputBind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
      inputBind[0].stride = sizeof(Vec3f) + sizeof(Vec3f) + sizeof(Vec2f);

      inputAttrib.resize(3);
      inputAttrib[0].binding = 0;
      inputAttrib[0].location = 0;
      inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      inputAttrib[0].offset = 0;
      inputAttrib[1].binding = 0;
      inputAttrib[1].location = 1;
      inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      inputAttrib[1].offset = sizeof(Vec3f);
      inputAttrib[2].binding = 0;
      inputAttrib[2].location = 2;
      inputAttrib[2].format = VK_FORMAT_R32G32B32_SFLOAT;
      inputAttrib[2].offset = sizeof(Vec3f) + sizeof(Vec3f);
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
    for (unsigned i = 0; i < 3 * mesh->tri_n; i++) {
      map.mem[i].vtx = mesh->vtx[mesh->tri[i]];
      map.mem[i].nrm = Vec3f(0.f);
      map.mem[i].tex = Vec2f(0.f);
    }
  }

  logger(0, "CreateRenderMesh");
  return renderMesh;
}


void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Vec4f& viewport, const Mat4f& MVP)
{
  if(!vanillaPipeline || vanillaPipeline.resource->pass != pass)
  {
    Vector<VkVertexInputBindingDescription> inputBind;
    Vector<VkVertexInputAttributeDescription> inputAttrib;
    VkPipelineLayoutCreateInfo pipeLayoutCI;
    VkDescriptorSetLayoutBinding descSetLayoutBind;
    VkDescriptorSetLayoutCreateInfo descLayoutCI;
    vanillaPipelineInfo(inputBind, inputAttrib, pipeLayoutCI, descSetLayoutBind, descLayoutCI);

    vanillaPipeline = vCtx->createPipeline(inputBind,
                                           inputAttrib,
                                           pipeLayoutCI,
                                           descLayoutCI,
                                           pass,
                                           vanillaShader);
    for (size_t i = 0; i < renaming.size(); i++) {
      renaming[i].vanillaDescSet = vCtx->createDescriptorSet(vanillaPipeline.resource->descLayout);
    }
  }
  auto & rename = renaming[renamingCurr];

  


  {
    MappedBuffer<ObjectBuffer> map(vCtx, rename.objectBuffer);
    map.mem->MVP = MVP;
  }
  vCtx->updateDescriptorSet(rename.vanillaDescSet, rename.objectBuffer);


  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);

  VkDescriptorSet desc_set[1] = { rename.vanillaDescSet.resource->descSet };
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);

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

  DebugScope debugScope(vCtx, cmdBuf, "Draw!");
  vkCmdDraw(cmdBuf, 3*renderMesh->tri_n, 1, 0, 0);
  

  renamingCurr = (renamingCurr + 1);
  if (renaming.size() <= renamingCurr) renamingCurr = 0;
}

void Renderer::destroyRenderMesh(RenderMesh* renderMesh)
{
  logger(0, "destroyRenderMesh");
  delete renderMesh;
}

