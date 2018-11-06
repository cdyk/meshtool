#include <cassert>
#include "Common.h"
#include "App.h"
#include "RenderSolid.h"
#include "Mesh.h"
#include "MeshIndexing.h"
#include "VulkanContext.h"
#include "RenderTextureManager.h"
#include "LinAlgOps.h"
#include "Half.h"
#include "VertexCache.h"

//#define TRASH_INDICES
#ifdef TRASH_INDICES
#include <algorithm>
#include <random>
#endif

struct ObjectBuffer
{
  Mat4f MVP;
  Vec4f Ncol0;
  Vec4f Ncol1;
  Vec4f Ncol2;
};

namespace {

  uint32_t solid_vert[] = {
#include "solid.vert.h"
  };


  uint32_t vanillaPS[]
#include "vanillaPS.glsl.h"
    ;

  uint32_t texturedPS[]
#include "texturedPS.glsl.h"
    ;

  struct Vertex
  {
    Vec3f pos;
    uint8_t nrm[4];
    uint16_t tex[2];
    uint8_t color[4];

    Vertex(const Vec3f& p,
           const Vec3f& n,
           const Vec2f& t,
           const uint32_t c)
    {
      pos = p;
      auto nn = normalize(n);
      nrm[0] = uint8_t(127.f*nn.x + 127.f);
      nrm[1] = uint8_t(127.f*nn.y + 127.f);
      nrm[2] = uint8_t(127.f*nn.z + 127.f);
      tex[0] = halfFromFloat(t.x);
      tex[1] = halfFromFloat(t.y);
      color[0] = (c >> 16) & 0xff;
      color[1] = (c >> 8) & 0xff;
      color[2] = (c) & 0xff;
      color[3] = 255;
    }
  };
  //static_assert(sizeof(Vertex) == 3 * 8);
 

  struct RGBA8
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
  static_assert(sizeof(RGBA8) == sizeof(uint32_t));

  struct Vec3fRGBA8
  {
    Vec3f p;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  };
  static_assert(sizeof(Vec3fRGBA8) == 4 * sizeof(uint32_t));

}

RenderSolid::RenderSolid(Logger logger, App* app) :
  logger(logger),
  app(app),
  textureManager(new RenderTextureManager(app->vCtx))
{

}

void RenderSolid::init()
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  vertexShader = resources->createShader(solid_vert, sizeof(solid_vert));
  solidShader = resources->createShader(vanillaPS, sizeof(vanillaPS));
  texturedShader = resources->createShader(texturedPS, sizeof(texturedPS));

  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->resources->createFence(true);
    renaming[i].objectBuffer = vCtx->resources->createUniformBuffer(sizeof(ObjectBuffer));
  }
  
  checkerTex = textureManager->loadTexture(TextureSource::Checker);
  colorGradientTex = textureManager->loadTexture(TextureSource::ColorGradient);

  VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = 16;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  texSampler = vCtx->resources->createSampler(samplerInfo);
}

RenderSolid::~RenderSolid()
{
}

void RenderSolid::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  newMeshData.clear();
  for (auto & mesh : meshes) {
    bool found = false;
    for (auto & item : meshData) {
      if (item.src == mesh) {
        newMeshData.pushBack(std::move(item));
        item.src = nullptr;
        found = true;
        break;
      }
    }
    if (!found) {
      newMeshData.pushBack(MeshData{ mesh });
      logger(0, "Created new Renderer.MeshData item.");
    }
    auto & meshData = newMeshData.back();
    if (meshData.geometryGeneration != mesh->geometryGeneration || meshData.colorGeneration != mesh->colorGeneration) {
      meshData.geometryGeneration = mesh->geometryGeneration;
      meshData.colorGeneration = mesh->colorGeneration;

      meshData.triangleCount = mesh->triCount;
      meshData.vtx = resources->createVertexDeviceBuffer(sizeof(Vertex) * 3 * meshData.triangleCount);

      Vector<uint32_t> indices;
      auto vtxStaging = resources->createStagingBuffer(meshData.vtx.resource->requestedSize);

      {
        MappedBuffer<Vertex> vtxMap(vCtx, vtxStaging);
        if (mesh->nrmCount) {
          if (mesh->texCount) {
            for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
              vtxMap.mem[i] = Vertex(mesh->vtx[mesh->triVtxIx[i]],
                                     mesh->nrm[mesh->triNrmIx[i]],
                                     10.f*mesh->tex[mesh->triTexIx[i]],
                                     mesh->currentColor[i / 3]);
            }
          }
          else {

            Vector<uint32_t> newVertices;

            uniqueIndices(logger, indices, newVertices, mesh->triVtxIx, mesh->triNrmIx, 3 * mesh->triCount);

            for (uint32_t i = 0; i < newVertices.size32(); i++) {
              auto ix = newVertices[i];
              vtxMap.mem[i] = Vertex(mesh->vtx[mesh->triVtxIx[ix]],
                                     mesh->nrm[mesh->triNrmIx[ix]],
                                     Vec2f(0.5f),
                                     0xdddddd);
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
                vtxMap.mem[3 * i + k] = Vertex(p[k],
                                               n,
                                               10.f*mesh->tex[mesh->triTexIx[3 * i + k]],
                                               mesh->currentColor[i]);
              }
            }
          }
          else {
            for (unsigned i = 0; i < mesh->triCount; i++) {
              Vec3f p[3];
              for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
              auto n = cross(p[1] - p[0], p[2] - p[0]);
              for (unsigned k = 0; k < 3; k++) {
                vtxMap.mem[3 * i + k] = Vertex(p[k],
                                               n,
                                               Vec2f(0.5f),
                                               mesh->currentColor[i]);
              }
            }
          }
        }
        vCtx->frameManager->copyBuffer(meshData.vtx, vtxStaging, meshData.vtx.resource->requestedSize);
      }

      if (indices.any()) {
#ifdef TRASH_INDICES
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle((Vec3f*)indices.begin(), (Vec3f*)indices.end(), g);
#endif
        Vector<uint32_t> reindices(indices.size());

        float fifo4, fifo8, fifo16, fifo32;
        getAverageCacheMissRatioPerTriangle(fifo4, fifo8, fifo16, fifo32, indices.data(), indices.size32());
        logger(0, "IN  AMCR FIFO4=%.2f, FIFO8=%.2f, FIFO16=%.2f, FIFO32=%.2f", fifo4, fifo8, fifo16, fifo32);
        //linearSpeedVertexCacheOptimisation(logger, reindices.data(), indices.data(), indices.size32());
        getAverageCacheMissRatioPerTriangle(fifo4, fifo8, fifo16, fifo32, reindices.data(), indices.size32());
        logger(0, "OPT AMCR FIFO4=%.2f, FIFO8=%.2f, FIFO16=%.2f, FIFO32=%.2f", fifo4, fifo8, fifo16, fifo32);
        indices.swap(reindices);

        meshData.indices = resources->createIndexDeviceBuffer(sizeof(uint32_t)*indices.size());
        auto stage = resources->createStagingBuffer(sizeof(uint32_t)*indices.size());
        {
          MappedBuffer<uint32_t> map(vCtx, stage);
          std::memcpy(map.mem, indices.data(), sizeof(uint32_t)*indices.size());
        }
        vCtx->frameManager->copyBuffer(meshData.indices, stage, sizeof(uint32_t)*indices.size());
      }
      else {
        meshData.indices = RenderBufferHandle();
      }

      logger(0, "RenderSolid: Updated MeshData item");
    }
  }
  meshData.swap(newMeshData);
  for (auto & item : newMeshData) {
    if (item.src == nullptr) continue;
    // just handles and those will destroy themselves.
    logger(0, "Destroyed Renderer.MeshData item.");
  }

  textureManager->houseKeep();
}


void RenderSolid::buildPipelines(RenderPassHandle pass)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  VkDescriptorSetLayoutBinding objBufLayoutBinding[1];
  objBufLayoutBinding[0] = {};
  objBufLayoutBinding[0].binding = 0;
  objBufLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  objBufLayoutBinding[0].descriptorCount = 1;
  objBufLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo objBufLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
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

  VkDescriptorSetLayoutCreateInfo objBufSamplerLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  objBufSamplerLayoutInfo.bindingCount = 2;
  objBufSamplerLayoutInfo.pBindings = objBufSamplerLayoutBinding;

  VkPipelineLayoutCreateInfo pipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pipeLayoutInfo.pushConstantRangeCount = 0;
  pipeLayoutInfo.pPushConstantRanges = NULL;

  VkPipelineRasterizationStateCreateInfo cullBackDepthBiasRasInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  cullBackDepthBiasRasInfo.polygonMode = VK_POLYGON_MODE_FILL;
  cullBackDepthBiasRasInfo.cullMode = VK_CULL_MODE_BACK_BIT;
  cullBackDepthBiasRasInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  cullBackDepthBiasRasInfo.depthClampEnable = VK_FALSE;
  cullBackDepthBiasRasInfo.rasterizerDiscardEnable = VK_FALSE;
  cullBackDepthBiasRasInfo.depthBiasEnable = VK_TRUE;
  cullBackDepthBiasRasInfo.depthBiasConstantFactor = 1;
  cullBackDepthBiasRasInfo.depthBiasClamp = 0;
  cullBackDepthBiasRasInfo.depthBiasSlopeFactor = 1;
  cullBackDepthBiasRasInfo.lineWidth = 1.0f;

  Vector<VkVertexInputAttributeDescription> inputAttrib(4);
  for (unsigned i = 0; i < inputAttrib.size(); i++) {
    inputAttrib[i] = { 0 };
    inputAttrib[i].location = i;
  }
  inputAttrib[0].offset = offsetof(Vertex, Vertex::pos);
  inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  inputAttrib[1].offset = offsetof(Vertex, Vertex::nrm);
  inputAttrib[1].format = VK_FORMAT_R8G8B8A8_UINT;
  inputAttrib[2].offset = offsetof(Vertex, Vertex::tex);
  inputAttrib[2].format = VK_FORMAT_R16G16_SFLOAT;
  inputAttrib[3].offset = offsetof(Vertex, Vertex::color);
  inputAttrib[3].format = VK_FORMAT_R8G8B8A8_UNORM;

  VkVertexInputBindingDescription inputBinding{};
  inputBinding.binding = 0;
  inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  inputBinding.stride = sizeof(Vertex);

  vanillaPipeline = resources->createPipeline({ inputBinding },
                                                inputAttrib,
                                                pipeLayoutInfo,
                                                objBufLayoutInfo,
                                                pass,
                                                { vertexShader, solidShader },
                                                cullBackDepthBiasRasInfo);
  texturedPipeline = resources->createPipeline({ inputBinding },
                                               inputAttrib,
                                               pipeLayoutInfo,
                                               objBufSamplerLayoutInfo,
                                               pass,
                                               { vertexShader, texturedShader },
                                               cullBackDepthBiasRasInfo);

  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].objBufDescSet = vCtx->resources->createDescriptorSet(vanillaPipeline.resource->descLayout);
    renaming[i].objBufSamplerDescSet = vCtx->resources->createDescriptorSet(texturedPipeline.resource->descLayout);
  }
  updateDescriptorSets();

}

void RenderSolid::updateDescriptorSets()
{
  auto * vCtx = app->vCtx;
  auto device = vCtx->device;

  inDescSet = texturing;

  VkWriteDescriptorSet writes[3];
  for (auto & rename : renaming) {

    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[0].dstSet = rename.objBufDescSet.resource->descSet;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &rename.objectBuffer.resource->descInfo;
    writes[0].dstArrayElement = 0;
    writes[0].dstBinding = 0;

    writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[1].dstSet = rename.objBufSamplerDescSet.resource->descSet;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].dstArrayElement = 0;
    writes[1].dstBinding = 0;
    writes[1].pBufferInfo = &rename.objectBuffer.resource->descInfo;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = inDescSet == Texturing::Checker ? checkerTex.resource->view.resource->view : colorGradientTex.resource->view.resource->view;
    imageInfo.sampler = texSampler.resource->sampler;

    writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[2].dstSet = rename.objBufSamplerDescSet.resource->descSet;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].dstArrayElement = 0;
    writes[2].dstBinding = 1;
    writes[2].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, ARRAYSIZE(writes), writes, 0, nullptr);
  }
}


void RenderSolid::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * vCtx = app->vCtx;

  if (!vanillaPipeline || vanillaPipeline.resource->pass != pass) buildPipelines(pass);

  if (inDescSet != texturing) updateDescriptorSets();

  VkViewport vp = {};
  vp.x = viewport.x;
  vp.y = viewport.y;
  vp.width = viewport.z;
  vp.height = viewport.w;
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;
  vkCmdSetViewport(cmdBuf, 0, 1, &vp);

  VkRect2D scissor;
  scissor.offset.x = int32_t(viewport.x);
  scissor.offset.y = int32_t(viewport.y);
  scissor.extent.width = uint32_t(viewport.z);
  scissor.extent.height = uint32_t(viewport.w);
  vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

  for (auto & item : meshData) {

    auto & rename = renaming[renameNext];
    renameNext = (renameNext + 1);
    if (renaming.size() <= renameNext) {
      renameNext = 0;
    }

    {
      MappedBuffer<ObjectBuffer> map(vCtx, rename.objectBuffer);
      map.mem->MVP = MVP;
      map.mem->Ncol0 = Vec4f(N.cols[0], 0.f);
      map.mem->Ncol1 = Vec4f(N.cols[1], 0.f);
      map.mem->Ncol2 = Vec4f(N.cols[2], 0.f);
    }

    VkBuffer buffers[1] = { item.vtx.resource->buffer };
    VkDeviceSize offsets[ARRAYSIZE(buffers)] = { 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, ARRAYSIZE(buffers), buffers, offsets);

    if (texturing != Texturing::None) {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipeLayout, 0, 1, &rename.objBufSamplerDescSet.resource->descSet, 0, NULL);
    }
    else {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    }
    if (item.indices) {
      vkCmdBindIndexBuffer(cmdBuf, item.indices.resource->buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(cmdBuf, 3 * item.triangleCount, 1, 0, 0, 0);
    }
    else {
      vkCmdDraw(cmdBuf, 3 * item.triangleCount, 1, 0, 0);
    }
  
  }
}

