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

  uint32_t texturedPS[]
#include "texturedPS.glsl.h"
    ;


  void vanillaPipelineInfo(VulkanContext* vCtx,
                           Vector<VkVertexInputBindingDescription>& inputBind,
                           Vector<VkVertexInputAttributeDescription>& inputAttrib,
                           VkPipelineLayoutCreateInfo& pipeLayoutCI)
  {
    {
      inputBind.resize(2);
      inputBind[0] = vCtx->infos->vertexInput.v32b;
      inputBind[0].binding = 0;
      inputBind[1] = vCtx->infos->vertexInput.v4b;
      inputBind[1].binding = 1;

      inputAttrib.resize(4);
      inputAttrib[0] = vCtx->infos->vertexInput.v3f_0_0b;
      inputAttrib[0].location = 0;
      inputAttrib[1] = vCtx->infos->vertexInput.v3f_1_12b;
      inputAttrib[1].location = 1;
      inputAttrib[2] = vCtx->infos->vertexInput.v2f_2_24b;
      inputAttrib[2].location = 2;
      inputAttrib[3] = vCtx->infos->vertexInput.v4u8_0b;
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
      inputBind[0] = vCtx->infos->vertexInput.v32b;
      inputBind[0].binding = 0;
      inputBind[1] = vCtx->infos->vertexInput.v4b;
      inputBind[1].binding = 1;

      inputAttrib.resize(2);
      inputAttrib[0] = vCtx->infos->vertexInput.v3f_0_0b;
      inputAttrib[0].location = 0;
      inputAttrib[1] = vCtx->infos->vertexInput.v4u8_0b;
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
    vanillaShader = vCtx->resources->createShader(stages);
  }
  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { flatVS, sizeof(flatVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { flatPS, sizeof(flatPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    flatShader = vCtx->resources->createShader(stages);
  }
  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { texturedPS, sizeof(texturedPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    texturedShader = vCtx->resources->createShader(stages);
  }

  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->resources->createFence(true);
    renaming[i].objectBuffer = vCtx->resources->createUniformBuffer(sizeof(ObjectBuffer));
  }
  



  uint32_t texW = 500;
  uint32_t texH = 500;

  auto stagingBuffer = vCtx->resources->createBuffer(texW*texH * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  {
    MappedBuffer<uint8_t> map(vCtx, stagingBuffer);
    for (unsigned j = 0; j < texH; j++) {
      for (unsigned i = 0; i < texW; i++) {
        auto g = ((i / (texW / 10) + j / (texH / 10))) & 1 ? 0 : 20;
        auto f = ((i / (texW / 2) + j / (texH / 2))) & 1 ? 150 : 235;
        map.mem[4 * (texW*j + i) + 0] = f + g;
        map.mem[4 * (texW*j + i) + 1] = f + g;
        map.mem[4 * (texW*j + i) + 2] = f + g;
        map.mem[4 * (texW*j + i) + 3] = 255;
      }
    }
  }

  {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent.width = static_cast<uint32_t>(texW);
    info.extent.height = static_cast<uint32_t>(texH);
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.flags = 0;

    texImage = vCtx->resources->createImage(info);
    auto viewInfo = vCtx->infos->imageView.baseLevel2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texImageView = vCtx->resources->createImageView(texImage, viewInfo);
    texSampler = vCtx->resources->createSampler(vCtx->infos->samplers.triLlinearRepeat);
  }


  vCtx->frameManager->transitionImageLayout(texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vCtx->frameManager->copyBufferToImage(texImage, stagingBuffer, texW, texH);
  vCtx->frameManager->transitionImageLayout(texImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

}

Renderer::~Renderer()
{
  houseKeep();
  logger(0, "%d unreleased buffers", renderMeshResources.getCount());
}

void Renderer::houseKeep()
{
  Vector<RenderMesh*> orphans;
  renderMeshResources.getOrphans(orphans);
  for (auto * r : orphans) {
    delete r;
  }
}


RenderMeshHandle Renderer::createRenderMesh(Mesh* mesh)
{
  struct VtxNrmTex
  {
    Vec3f vtx;
    Vec3f nrm;
    Vec2f tex;
  };

  auto renderMeshHandle = renderMeshResources.createResource();
  auto * renderMesh = renderMeshHandle.resource;

  renderMesh->mesh = mesh;
  renderMesh->tri_n = mesh->triCount;
  
  renderMesh->vtxNrmTex = vCtx->resources->createVertexBuffer(sizeof(VtxNrmTex) * 3 * renderMesh->tri_n);

  {
    MappedBuffer<VtxNrmTex> map(vCtx, renderMesh->vtxNrmTex);
    if (mesh->nrmCount) {
      for (unsigned i = 0; i < 3*mesh->triCount; i++) {
        map.mem[i].vtx = mesh->vtx[mesh->triVtxIx[i]];
        map.mem[i].nrm = mesh->nrm[mesh->triNrmIx[i]];
        map.mem[i].tex = Vec2f(mesh->vtx[mesh->triVtxIx[i]].data);
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
          map.mem[3 * i + k].tex = Vec2f(p[k].x, p[k].y);
        }
      }
    }
  }

  renderMesh->color = vCtx->resources->createVertexBuffer(sizeof(uint32_t) * 3 * renderMesh->tri_n);
  updateRenderMeshColor(renderMesh);

  logger(0, "CreateRenderMesh");
  return renderMeshHandle;
}

void Renderer::updateRenderMeshColor(RenderMeshHandle renderMesh)
{
  auto * rm = renderMesh.resource;
  MappedBuffer<uint32_t> map(vCtx, rm->color);
  for (unsigned i = 0; i < rm->mesh->triCount; i++) {
    auto color = rm->mesh->currentColor[i];
    map.mem[3 * i + 0] = color;
    map.mem[3 * i + 1] = color;
    map.mem[3 * i + 2] = color;
  }
  logger(0, "Updated mesh color");
}


void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMeshHandle renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * rm = renderMesh.resource;
  if(!vanillaPipeline || vanillaPipeline.resource->pass != pass)
  {
    VkDescriptorSetLayoutBinding objBufLayoutBinding[1];
    objBufLayoutBinding[0] = {};
    objBufLayoutBinding[0].binding = 0;
    objBufLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBufLayoutBinding[0].descriptorCount = 1;
    objBufLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo objBufLayoutInfo{};
    objBufLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objBufLayoutInfo.bindingCount = 1;
    objBufLayoutInfo.pBindings = objBufLayoutBinding;

    VkDescriptorSetLayoutBinding objBufSamplerLayoutBinding[2];
    objBufSamplerLayoutBinding[0] = {};
    objBufSamplerLayoutBinding[0].binding = 0;
    objBufSamplerLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    objBufSamplerLayoutBinding[0].descriptorCount = 1;
    objBufSamplerLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    objBufSamplerLayoutBinding[1].binding = 1;
    objBufSamplerLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    objBufSamplerLayoutBinding[1].descriptorCount = 1;
    objBufSamplerLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    objBufSamplerLayoutBinding[1].pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo objBufSamplerLayoutInfo{};
    objBufSamplerLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objBufSamplerLayoutInfo.bindingCount = 2;
    objBufSamplerLayoutInfo.pBindings = objBufSamplerLayoutBinding;



    Vector<VkVertexInputBindingDescription> inputBind;
    Vector<VkVertexInputAttributeDescription> inputAttrib;
    VkPipelineLayoutCreateInfo pipeLayoutCI;
    vanillaPipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI);

    vanillaPipeline = vCtx->resources->createPipeline(inputBind,
                                                      inputAttrib,
                                                      pipeLayoutCI,
                                                      objBufLayoutInfo,
                                                      pass,
                                                      vanillaShader,
                                                      vCtx->infos->pipelineRasterization.cullBackDepthBias);

    texturedPipeline = vCtx->resources->createPipeline(inputBind,
                                                       inputAttrib,
                                                       pipeLayoutCI,
                                                       objBufSamplerLayoutInfo,
                                                       pass,
                                                       texturedShader,
                                                       vCtx->infos->pipelineRasterization.cullBackDepthBias);



    wirePipelineInfo(vCtx, inputBind, inputAttrib, pipeLayoutCI);
    wireFrontFacePipeline = vCtx->resources->createPipeline(inputBind,
                                                            inputAttrib,
                                                            pipeLayoutCI,
                                                            objBufLayoutInfo,
                                                            pass,
                                                            flatShader,
                                                            vCtx->infos->pipelineRasterization.cullBackLine);

    wireBothFacesPipeline = vCtx->resources->createPipeline(inputBind,
                                                            inputAttrib,
                                                            pipeLayoutCI,
                                                            objBufLayoutInfo,
                                                            pass,
                                                            flatShader,
                                                            vCtx->infos->pipelineRasterization.cullNoneLine);


    for (size_t i = 0; i < renaming.size(); i++) {
      renaming[i].objBufDescSet = vCtx->resources->createDescriptorSet(vanillaPipeline.resource->descLayout);
      renaming[i].objBufSamplerDescSet = vCtx->resources->createDescriptorSet(texturedPipeline.resource->descLayout);
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
  {
    VkWriteDescriptorSet writes[1];
    writes[0] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = nullptr;
    writes[0].dstSet = rename.objBufDescSet.resource->descSet;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &rename.objectBuffer.resource->descInfo;
    writes[0].dstArrayElement = 0;
    writes[0].dstBinding = 0;
    vkUpdateDescriptorSets(vCtx->device, 1, writes, 0, nullptr);
  }
  {
    VkWriteDescriptorSet writes[2];
    writes[0] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = rename.objBufSamplerDescSet.resource->descSet;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].dstArrayElement = 0;
    writes[0].dstBinding = 0;
    writes[0].pBufferInfo = &rename.objectBuffer.resource->descInfo;

    // Fixme: this is only needed once    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texImageView.resource->view;
    imageInfo.sampler = texSampler.resource->sampler;

    writes[1] = {};
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = rename.objBufSamplerDescSet.resource->descSet;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].dstArrayElement = 0;
    writes[1].dstBinding = 1;
    writes[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(vCtx->device, 2, writes, 0, nullptr);
  }


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
    VkBuffer buffers[2] = { rm->vtxNrmTex.resource->buffer, rm->color.resource->buffer };
    VkDeviceSize offsets[2] = { 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 2, buffers, offsets);
  }

  if(solid) {
    if (textured) {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipeLayout, 0, 1, &rename.objBufSamplerDescSet.resource->descSet, 0, NULL);

    }
    else {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    }
    vkCmdDraw(cmdBuf, 3 * rm->tri_n, 1, 0, 0);
  }

  if(outlines) {
    if (solid) {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, wireFrontFacePipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    }
    else {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, wireBothFacesPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    }
    vkCmdDraw(cmdBuf, 3 * rm->tri_n, 1, 0, 0);
  }


  renamingCurr = (renamingCurr + 1);
  if (renaming.size() <= renamingCurr) renamingCurr = 0;
}

