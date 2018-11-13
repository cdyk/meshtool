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
#include "ShaderStructs.h"

//#define TRASH_INDICES
#ifdef TRASH_INDICES
#include <algorithm>
#include <random>
#endif


namespace {

  uint32_t vanilla_vert[] = {
#include "vanilla.vert.h"
  };

  uint32_t vanilla_frag[] = {
#include "vanilla.frag.h"
  };

  uint32_t textured_frag[]
#include "textured.frag.h"
  ;

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

  vertexShader = resources->createShader(vanilla_vert, sizeof(vanilla_vert));
  solidShader = resources->createShader(vanilla_frag, sizeof(vanilla_frag));
  texturedShader = resources->createShader(textured_frag, sizeof(textured_frag));
  
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
      meshData.vtx = resources->createStorageBuffer(sizeof(Vertex) * 3 * meshData.triangleCount);

      Vector<uint32_t> indices;
      auto vtxStaging = resources->createStagingBuffer(meshData.vtx.resource->requestedSize);

      {
        auto * mem = (Vertex*)vtxStaging.resource->hostPtr;
        if (mesh->nrmCount) {
          if (false && mesh->texCount) {
            for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
              mem[i] = Vertex(mesh->vtx[mesh->triVtxIx[i]],
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
              mem[i] = Vertex(mesh->vtx[mesh->triVtxIx[ix]],
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
                mem[3 * i + k] = Vertex(p[k],
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
                mem[3 * i + k] = Vertex(p[k],
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
        linearSpeedVertexCacheOptimisation(logger, reindices.data(), indices.data(), indices.size32());
        getAverageCacheMissRatioPerTriangle(fifo4, fifo8, fifo16, fifo32, reindices.data(), indices.size32());
        logger(0, "OPT AMCR FIFO4=%.2f, FIFO8=%.2f, FIFO16=%.2f, FIFO32=%.2f", fifo4, fifo8, fifo16, fifo32);
        indices.swap(reindices);

        meshData.indices = resources->createIndexDeviceBuffer(sizeof(uint32_t)*indices.size());
        auto stage = resources->createStagingBuffer(sizeof(uint32_t)*indices.size());
        std::memcpy(stage.resource->hostPtr, indices.data(), sizeof(uint32_t)*indices.size());
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

  VkDescriptorSetLayoutBinding objBufLayoutBinding[2];
  objBufLayoutBinding[0] = {};
  objBufLayoutBinding[0].binding = 0;
  objBufLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  objBufLayoutBinding[0].descriptorCount = 1;
  objBufLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  objBufLayoutBinding[1] = {};
  objBufLayoutBinding[1].binding = 1;
  objBufLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  objBufLayoutBinding[1].descriptorCount = 1;
  objBufLayoutBinding[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo objBufLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  objBufLayoutInfo.bindingCount = ARRAYSIZE(objBufLayoutBinding);
  objBufLayoutInfo.pBindings = objBufLayoutBinding;

  VkDescriptorSetLayoutBinding objBufSamplerLayoutBinding[3];
  objBufSamplerLayoutBinding[0] = objBufLayoutBinding[0];
  objBufSamplerLayoutBinding[1] = objBufLayoutBinding[1];
  objBufSamplerLayoutBinding[2] = {};
  objBufSamplerLayoutBinding[2].binding = 2;
  objBufSamplerLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  objBufSamplerLayoutBinding[2].descriptorCount = 1;
  objBufSamplerLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo objBufSamplerLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  objBufSamplerLayoutInfo.bindingCount = ARRAYSIZE(objBufSamplerLayoutBinding);
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

  vanillaPipeline = resources->createPipeline({ }, { },
                                              pipeLayoutInfo,
                                              objBufLayoutInfo,
                                              pass,
                                              { vertexShader, solidShader },
                                              cullBackDepthBiasRasInfo);
  texturedPipeline = resources->createPipeline({ }, { },
                                               pipeLayoutInfo,
                                               objBufSamplerLayoutInfo,
                                               pass,
                                               { vertexShader, texturedShader },
                                               cullBackDepthBiasRasInfo);
}


void RenderSolid::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * vCtx = app->vCtx;
  auto device = vCtx->device;
  auto * frameManager = vCtx->frameManager;

  if (!vanillaPipeline || vanillaPipeline.resource->pass != pass) buildPipelines(pass);

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

    VkDescriptorBufferInfo objectBufferInfo;
    auto* objectBuffer = (ObjectBuffer*)vCtx->frameManager->allocUniformStore(objectBufferInfo, sizeof(ObjectBuffer));
    objectBuffer->MVP = MVP;
    objectBuffer->Ncol0 = Vec4f(N.cols[0], 0.f);
    objectBuffer->Ncol1 = Vec4f(N.cols[1], 0.f);
    objectBuffer->Ncol2 = Vec4f(N.cols[2], 0.f);


    if (texturing == Texturing::None) {

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipe);

      VkDescriptorSet set = frameManager->allocDescriptorSet(vanillaPipeline);

      VkWriteDescriptorSet writes[2];
      for (size_t i = 0; i < ARRAYSIZE(writes); i++) {
        writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writes[i].dstSet = set;
        writes[i].descriptorCount = 1;
        writes[i].dstBinding = uint32_t(i);
      }
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[0].pBufferInfo = &objectBufferInfo;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[1].pBufferInfo = &item.vtx.resource->descInfo;
      vkUpdateDescriptorSets(device, ARRAYSIZE(writes), writes, 0, nullptr);

      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vanillaPipeline.resource->pipeLayout, 0, 1, &set, 0, NULL);
    }
    else {

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipe);

      VkDescriptorSet set = frameManager->allocDescriptorSet(texturedPipeline);

      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = inDescSet == Texturing::Checker ? checkerTex.resource->view.resource->view : colorGradientTex.resource->view.resource->view;
      imageInfo.sampler = texSampler.resource->sampler;

      VkWriteDescriptorSet writes[3];
      for (size_t i = 0; i < ARRAYSIZE(writes); i++) {
        writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writes[i].dstSet = set;
        writes[i].descriptorCount = 1;
        writes[i].dstBinding = uint32_t(i);
      }
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[0].pBufferInfo = &objectBufferInfo;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[1].pBufferInfo = &item.vtx.resource->descInfo;
      writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writes[2].pImageInfo = &imageInfo;
      vkUpdateDescriptorSets(device, ARRAYSIZE(writes), writes, 0, nullptr);

      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, texturedPipeline.resource->pipeLayout, 0, 1, &set, 0, NULL);
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

