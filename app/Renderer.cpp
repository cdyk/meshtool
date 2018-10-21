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

  uint32_t coordSysVS[]
#include "coordSysVS.glsl.h"
    ;

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

  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { coordSysVS, sizeof(coordSysVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { flatPS, sizeof(flatPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    coordSys.shader = vCtx->resources->createShader(stages);
  }


  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->resources->createFence(true);
    renaming[i].objectBuffer = vCtx->resources->createUniformBuffer(sizeof(ObjectBuffer));
  }
  
  coordSys.vtxCol = vCtx->resources->createVertexDeviceBuffer(2*sizeof(Vec3f) * 6);
  {
    auto coordSysStaging = vCtx->resources->createStagingBuffer(coordSys.vtxCol.resource->requestedSize);
    {
      MappedBuffer<Vec3f> vtxMap(vCtx, coordSysStaging);
      vtxMap.mem[0] = Vec3f(0, 0, 0); vtxMap.mem[1] = Vec3f(1, 0, 0);
      vtxMap.mem[2] = Vec3f(1, 0, 0); vtxMap.mem[3] = Vec3f(1, 0, 0);
      vtxMap.mem[4] = Vec3f(0, 0, 0); vtxMap.mem[5] = Vec3f(0, 1, 0);
      vtxMap.mem[6] = Vec3f(0, 1, 0); vtxMap.mem[7] = Vec3f(0, 1, 0);
      vtxMap.mem[8] = Vec3f(0, 0, 0); vtxMap.mem[9] = Vec3f(0.3f, 0.3f, 1);
      vtxMap.mem[10] = Vec3f(0, 0, 1); vtxMap.mem[11] = Vec3f(0.3f, 0.3f, 1);
    }
    vCtx->frameManager->copyBuffer(coordSys.vtxCol, coordSysStaging, coordSys.vtxCol.resource->requestedSize);
  }

  uint32_t texW = 500;
  uint32_t texH = 500;


  auto checkerStagingBuffer = vCtx->resources->createStagingBuffer(texW*texH * 4);
  {
    MappedBuffer<uint8_t> map(vCtx, checkerStagingBuffer);
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

  auto colorGradientStagingBuffer = vCtx->resources->createStagingBuffer(texW*texH * 4);
  {
    MappedBuffer<uint8_t> map(vCtx, colorGradientStagingBuffer);
    for (unsigned j = 0; j < texH; j++) {
      for (unsigned i = 0; i < texW; i++) {
        auto g = ((i / (texW / 10) + j / (texH / 10))) & 1 ? 0 : 20;
        auto f = ((i / (texW / 2) + j / (texH / 2))) & 1 ? 150 : 235;
        map.mem[4 * (texW*j + i) + 0] = (255 * (i)) / (texW);
        map.mem[4 * (texW*j + i) + 1] = (255 * (j)) / (texH);
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

    auto viewInfo = vCtx->infos->imageView.baseLevel2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    checkerTexImage = vCtx->resources->createImage(info);
    checkerTexImageView = vCtx->resources->createImageView(checkerTexImage, viewInfo);

    vCtx->frameManager->transitionImageLayout(checkerTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vCtx->frameManager->copyBufferToImage(checkerTexImage, checkerStagingBuffer, texW, texH);
    vCtx->frameManager->transitionImageLayout(checkerTexImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    colorGradientTexImage = vCtx->resources->createImage(info);
    colorGradientTexImageView = vCtx->resources->createImageView(colorGradientTexImage, viewInfo);

    vCtx->frameManager->transitionImageLayout(colorGradientTexImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vCtx->frameManager->copyBufferToImage(colorGradientTexImage, colorGradientStagingBuffer, texW, texH);
    vCtx->frameManager->transitionImageLayout(colorGradientTexImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

     texSampler = vCtx->resources->createSampler(vCtx->infos->samplers.triLlinearRepeat);
  }
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

  auto renderMeshHandle = renderMeshResources.createResource();
  auto * renderMesh = renderMeshHandle.resource;

  renderMesh->mesh = mesh;
  renderMesh->tri_n = mesh->triCount;
  
  renderMesh->vtx = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * renderMesh->tri_n);
  renderMesh->nrm = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * renderMesh->tri_n);
  renderMesh->tan = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * renderMesh->tri_n);
  renderMesh->bnm = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * renderMesh->tri_n);
  renderMesh->tex = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec2f) * 3 * renderMesh->tri_n);
  renderMesh->col = vCtx->resources->createVertexDeviceBuffer(sizeof(uint32_t) * 3 * renderMesh->tri_n);

  {
    auto vtxStaging = vCtx->resources->createStagingBuffer(renderMesh->vtx.resource->requestedSize);
    auto nrmStaging = vCtx->resources->createStagingBuffer(renderMesh->nrm.resource->requestedSize);
    auto tanStaging = vCtx->resources->createStagingBuffer(renderMesh->tan.resource->requestedSize);
    auto bnmStaging = vCtx->resources->createStagingBuffer(renderMesh->bnm.resource->requestedSize);
    auto texStaging = vCtx->resources->createStagingBuffer(renderMesh->tex.resource->requestedSize);
    {
      MappedBuffer<Vec3f> vtxMap(vCtx, vtxStaging);
      MappedBuffer<Vec3f> nrmMap(vCtx, nrmStaging);
      MappedBuffer<Vec3f> tanMap(vCtx, tanStaging);
      MappedBuffer<Vec3f> bnmMap(vCtx, bnmStaging);
      MappedBuffer<Vec2f> texMap(vCtx, texStaging);

      if (mesh->nrmCount) {
        if (mesh->texCount) {
          for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
            vtxMap.mem[i] = mesh->vtx[mesh->triVtxIx[i]];
            nrmMap.mem[i] = mesh->nrm[mesh->triNrmIx[i]];
            texMap.mem[i] = mesh->tex[mesh->triTexIx[i]];
            tanMap.mem[i] = Vec3f(0.f);
            bnmMap.mem[i] = Vec3f(0.f);
          }
        }
        else {
          for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
            vtxMap.mem[i] = mesh->vtx[mesh->triVtxIx[i]];
            nrmMap.mem[i] = mesh->nrm[mesh->triNrmIx[i]];
            tanMap.mem[i] = Vec3f(0.f);
            bnmMap.mem[i] = Vec3f(0.f);
            texMap.mem[i] = Vec2f(0.5f);
          }
        }
      }
      else {
        if (mesh->texCount) {
          for (unsigned i = 0; i < mesh->triCount; i++) {
            Vec3f p[3];
            for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
            auto n = cross(p[1] - p[0], p[2] - p[0]);
            for (unsigned k = 0; k < 3; k++) {
              vtxMap.mem[3 * i + k] = p[k];
              nrmMap.mem[3 * i + k] = n;
              texMap.mem[3 * i + k] = 10.f*mesh->tex[mesh->triTexIx[3 * i + k]];
              tanMap.mem[3 * i + k] = Vec3f(0.f);
              bnmMap.mem[3 * i + k] = Vec3f(0.f);
            }
          }
        }
        else {
          for (unsigned i = 0; i < mesh->triCount; i++) {
            Vec3f p[3];
            for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
            auto n = cross(p[1] - p[0], p[2] - p[0]);
            for (unsigned k = 0; k < 3; k++) {
              vtxMap.mem[3 * i + k] = p[k];
              nrmMap.mem[3 * i + k] = n;
              texMap.mem[3 * i + k] = Vec2f(0.5f);
              tanMap.mem[3 * i + k] = Vec3f(0.f);
              bnmMap.mem[3 * i + k] = Vec3f(0.f);
            }
          }
        }
      }
      vCtx->frameManager->copyBuffer(renderMesh->vtx, vtxStaging, renderMesh->vtx.resource->requestedSize);
      vCtx->frameManager->copyBuffer(renderMesh->nrm, nrmStaging, renderMesh->nrm.resource->requestedSize);
      vCtx->frameManager->copyBuffer(renderMesh->tan, tanStaging, renderMesh->tan.resource->requestedSize);
      vCtx->frameManager->copyBuffer(renderMesh->bnm, bnmStaging, renderMesh->bnm.resource->requestedSize);
      vCtx->frameManager->copyBuffer(renderMesh->tex, texStaging, renderMesh->tex.resource->requestedSize);
    }
  }

  updateRenderMeshColor(renderMesh);

  logger(0, "CreateRenderMesh");
  return renderMeshHandle;
}

void Renderer::updateRenderMeshColor(RenderMeshHandle renderMesh)
{
  auto * rm = renderMesh.resource;
  auto stagingBuf = vCtx->resources->createStagingBuffer(rm->col.resource->requestedSize);
  {
    MappedBuffer<uint32_t> map(vCtx, stagingBuf);
    for (unsigned i = 0; i < rm->mesh->triCount; i++) {
      auto color = rm->mesh->currentColor[i];
      map.mem[3 * i + 0] = color;
      map.mem[3 * i + 1] = color;
      map.mem[3 * i + 2] = color;
    }
  }
  vCtx->frameManager->copyBuffer(rm->col, stagingBuf, rm->col.resource->requestedSize);
  logger(0, "Updated mesh color");
}

void Renderer::buildPipelines(RenderPassHandle pass)
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



  
  VkPipelineLayoutCreateInfo pipeLayoutInfo{};
  pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeLayoutInfo.pNext = NULL;
  pipeLayoutInfo.pushConstantRangeCount = 0;
  pipeLayoutInfo.pPushConstantRanges = NULL;

  {
    Vector<VkVertexInputAttributeDescription> inputAttrib(4);
    for (unsigned i = 0; i < inputAttrib.size(); i++) {
      inputAttrib[i] = { 0 };
      inputAttrib[i].location = i;
      inputAttrib[i].binding = i;
    }
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[2].format = VK_FORMAT_R32G32_SFLOAT;
    inputAttrib[3].format = VK_FORMAT_R8G8B8A8_UNORM;

    Vector<VkVertexInputBindingDescription> inputBind(4);
    for (unsigned i = 0; i < inputBind.size(); i++) {
      inputBind[i] = { 0 };
      inputBind[i].binding = i;
      inputBind[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    inputBind[0].stride = sizeof(Vec3f);
    inputBind[1].stride = sizeof(Vec3f);
    inputBind[2].stride = sizeof(Vec2f);
    inputBind[3].stride = sizeof(uint32_t);
    vanillaPipeline = vCtx->resources->createPipeline(inputBind,
                                                      inputAttrib,
                                                      pipeLayoutInfo,
                                                      objBufLayoutInfo,
                                                      pass,
                                                      vanillaShader,
                                                      vCtx->infos->pipelineRasterization.cullBackDepthBias);
  }

  {
    Vector<VkVertexInputAttributeDescription> inputAttrib(4);
    for (unsigned i = 0; i < inputAttrib.size(); i++) {
      inputAttrib[i] = { 0 };
      inputAttrib[i].location = i;
      inputAttrib[i].binding = i;
    }
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[2].format = VK_FORMAT_R32G32_SFLOAT;
    inputAttrib[3].format = VK_FORMAT_R8G8B8A8_UNORM;

    Vector<VkVertexInputBindingDescription> inputBind(4);
    for (unsigned i = 0; i < inputBind.size(); i++) {
      inputBind[i] = { 0 };
      inputBind[i].binding = i;
      inputBind[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    inputBind[0].stride = sizeof(Vec3f);
    inputBind[1].stride = sizeof(Vec3f);
    inputBind[2].stride = sizeof(Vec2f);
    inputBind[3].stride = sizeof(uint32_t);
    texturedPipeline = vCtx->resources->createPipeline(inputBind,
                                                       inputAttrib,
                                                       pipeLayoutInfo,
                                                       objBufSamplerLayoutInfo,
                                                       pass,
                                                       texturedShader,
                                                       vCtx->infos->pipelineRasterization.cullBackDepthBias);
  }

  {
    Vector<VkVertexInputAttributeDescription> inputAttrib(2);
    for (unsigned i = 0; i < inputAttrib.size(); i++) {
      inputAttrib[i] = { 0 };
      inputAttrib[i].location = i;
      inputAttrib[i].binding = i;
    }
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].format = VK_FORMAT_R8G8B8A8_UNORM;

    Vector<VkVertexInputBindingDescription> inputBind(2);
    for (unsigned i = 0; i < inputBind.size(); i++) {
      inputBind[i] = { 0 };
      inputBind[i].binding = i;
      inputBind[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    }
    inputBind[0].stride = sizeof(Vec3f);
    inputBind[1].stride = sizeof(uint32_t);

    wireFrontFacePipeline = vCtx->resources->createPipeline(inputBind,
                                                            inputAttrib,
                                                            pipeLayoutInfo,
                                                            objBufLayoutInfo,
                                                            pass,
                                                            flatShader,
                                                            vCtx->infos->pipelineRasterization.cullBackLine);

    wireBothFacesPipeline = vCtx->resources->createPipeline(inputBind,
                                                            inputAttrib,
                                                            pipeLayoutInfo,
                                                            objBufLayoutInfo,
                                                            pass,
                                                            flatShader,
                                                            vCtx->infos->pipelineRasterization.cullNoneLine);
  }

  {
    Vector<VkVertexInputAttributeDescription> inputAttrib(6);
    inputAttrib[0] = { 0 };
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1] = { 0 };
    inputAttrib[1].location = 1;
    inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].offset = sizeof(Vec3f);
    for (unsigned i = 2; i < inputAttrib.size(); i++) {
      inputAttrib[i] = { 0 };
      inputAttrib[i].location = i;
      inputAttrib[i].binding = i - 1;
      inputAttrib[i].format = VK_FORMAT_R32G32B32_SFLOAT;
    }

    Vector<VkVertexInputBindingDescription> inputBind(5);
    inputBind[0] = { 0 };
    inputBind[0].stride = sizeof(Vec3f) + sizeof(Vec3f);
    inputBind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    for (unsigned i = 1; i < inputBind.size(); i++) {
      inputBind[i] = { 0 };
      inputBind[i].binding = i;
      inputBind[i].stride = sizeof(Vec3f);
      inputBind[i].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    }
    coordSys.pipeline = vCtx->resources->createPipeline(inputBind,
                                                        inputAttrib,
                                                        pipeLayoutInfo,
                                                        objBufLayoutInfo,
                                                        pass,
                                                        coordSys.shader,
                                                        vCtx->infos->pipelineRasterization.cullNoneSolid,
                                                        VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
  }


  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].objBufDescSet = vCtx->resources->createDescriptorSet(vanillaPipeline.resource->descLayout);
    renaming[i].objBufSamplerDescSet = vCtx->resources->createDescriptorSet(texturedPipeline.resource->descLayout);
  }

}

void Renderer::drawRenderMesh(VkCommandBuffer cmdBuf, RenderPassHandle pass, RenderMeshHandle renderMesh, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * rm = renderMesh.resource;
  if (!vanillaPipeline || vanillaPipeline.resource->pass != pass) buildPipelines(pass);

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
    imageInfo.imageView = texturing == Texturing::Checker ? checkerTexImageView.resource->view : colorGradientTexImageView.resource->view;
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
    VkBuffer buffers[4] = {
      rm->vtx.resource->buffer,
      rm->nrm.resource->buffer,
      rm->tex.resource->buffer,
      rm->col.resource->buffer };
    VkDeviceSize offsets[4] = { 0, 0, 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 4, buffers, offsets);
  }

  if(solid) {
    if (texturing != Texturing::None) {
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

  if(true) {
    VkBuffer buffers[5] = {
      coordSys.vtxCol.resource->buffer,
      rm->vtx.resource->buffer,
      rm->tan.resource->buffer,
      rm->bnm.resource->buffer,
      rm->nrm.resource->buffer,
    };
    VkDeviceSize offsets[5] = { 0, 0, 0, 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 5, buffers, offsets);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, coordSys.pipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, coordSys.pipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    vkCmdDraw(cmdBuf, 6, 3 * rm->tri_n, 0, 0);

  }



  renamingCurr = (renamingCurr + 1);
  if (renaming.size() <= renamingCurr) renamingCurr = 0;
}

