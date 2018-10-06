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

  objectBuffer = vCtx->createUniformBuffer(sizeof(ObjectBuffer));


/* vanillaPipeline = new RenderPipeline;
  vCtx->buildShader(vanillaPipeline->shader, stages, 2);


  */

  //buildPipeline(vanillaPipeline, device, vanillaShaders,
  //              Buffer<VkVertexInputBindingDescription>& inputBind,
  //              Buffer<VkVertexInputAttributeDescription>& inputAttrib,
  //              VkPipelineLayout pipelineLayout,
  //              VkRenderPass renderPass);


  //execute_end_command_buffer(info);
//execute_queue_command_buffer(info);

  

  logger(0, "moo %d", sizeof(vanillaVS));


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

#if 0

  VtxNrmTex *mem;
  auto rv = vkMapMemory(vCtx->device, renderMesh->vtxNrmTex.mem, 0, renderMesh->vtxNrmTex.size, 0, (void **)&mem);
  assert(rv == VK_SUCCESS);

  vkUnmapMemory(vCtx->device, renderMesh->vtxNrmTex.mem);

#endif
  logger(0, "CreateRenderMesh");
  return nullptr;
}

void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMesh* renderMesh, const Mat4f& MVP)
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
    vanillaDescSet = vCtx->createDescriptorSet(vanillaPipeline.resource->descLayout);
  }

  {
    MappedBuffer<ObjectBuffer> map(vCtx, objectBuffer);
    map.mem->MVP = MVP;
  }
  vCtx->updateDescriptorSet(vanillaDescSet, objectBuffer);


  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);

  //VkDescriptorSet desc_set[1] = { g_DescriptorSet };
  //vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, desc_set, 0, NULL);

  int a = 2;
  {
    //VkDescriptorSet desc_set[1] = { g_DescriptorSet };
    //vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_PipelineLayout, 0, 1, desc_set, 0, NULL);
  }


#if 0

  /* We cannot bind the vertex buffer until we begin a renderpass */
  VkClearValue clear_values[2];
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;

  VkSemaphore imageAcquiredSemaphore;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  res = vkCreateSemaphore(info.device, &imageAcquiredSemaphoreCreateInfo, NULL, &imageAcquiredSemaphore);
  assert(res == VK_SUCCESS);

  // Get the index of the next available swapchain image:
  res = vkAcquireNextImageKHR(info.device, info.swap_chain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE,
                              &info.current_buffer);
  // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
  // return codes
  assert(res == VK_SUCCESS);

  VkRenderPassBeginInfo rp_begin = {};
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = NULL;
  rp_begin.renderPass = info.render_pass;
  rp_begin.framebuffer = info.framebuffers[info.current_buffer];
  rp_begin.renderArea.offset.x = 0;
  rp_begin.renderArea.offset.y = 0;
  rp_begin.renderArea.extent.width = info.width;
  rp_begin.renderArea.extent.height = info.height;
  rp_begin.clearValueCount = 2;
  rp_begin.pClearValues = clear_values;

  vkCmdBeginRenderPass(info.cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindVertexBuffers(info.cmd, 0,             /* Start Binding */
                         1,                       /* Binding Count */
                         &info.vertex_buffer.buf, /* pBuffers */
                         offsets);                /* pOffsets */

  vkCmdEndRenderPass(info.cmd);
  execute_end_command_buffer(info);
  execute_queue_command_buffer(info);
#endif
}

void Renderer::destroyRenderMesh(RenderMesh* renderMesh)
{
  logger(0, "destroyRenderMesh");
  delete renderMesh;
}

