#include <cassert>
#include "Common.h"
#include "App.h"
#include "RenderSolidMS.h"
#include "Mesh.h"
#include "MeshIndexing.h"
#include "VulkanContext.h"
#include "RenderTextureManager.h"
#include "LinAlgOps.h"
#include "Half.h"
#include "VertexCache.h"
#include "ShaderStructs.h"
#include "Bounds.h"

//#define TRASH_INDICES
#ifdef TRASH_INDICES
#include <algorithm>
#include <random>
#endif


namespace {

  uint32_t vanilla_task[] = {
#include "vanilla.task.h"
  };

  uint32_t vanilla_mesh[] = {
#include "vanilla.mesh.h"
  };

  uint32_t vanilla_frag[] = {
#include "vanilla.frag.h"
  };

  uint32_t textured_frag[] = {
#include "textured.frag.h"
  };

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


  void storeMeshlet(Vector<uint32_t>& meshletData, Vector<Meshlet>& meshlets, const uint8_t* localindices, const uint32_t offset, const uint32_t meshletIndices)
  {
    Meshlet meshlet{};
    meshlet.offset = offset;
    meshlet.vertexCount = meshletData.size32() - offset;
    meshlet.triangleCount = meshletIndices / 3;
    meshlets.pushBack(meshlet);

    auto o = meshletData.size32();
    meshletData.resize(o + (meshletIndices + 3) / 4);

    auto * p = (uint8_t*)(meshletData.data() + o);
    auto * q = (uint8_t*)(meshletData.data() + meshletData.size());
    for (uint32_t i = 0; i < meshletIndices; i++) {
      *p++ = localindices[i];
    }
    while (p < q) *p++ = 0;
  }

  void buildMeshlets(Vector<uint32_t>& meshletData, Vector<Meshlet>& meshlets, const Vector<uint32_t>& indices)
  {
    meshletData.clear();
    meshlets.clear();


    uint32_t offset = 0;
    Map localVertices;

    uint32_t meshletIndices = 0;
    uint8_t localindices[126 * 3 + 3];
    for (uint32_t iIn = 0; iIn < indices.size32(); ) {

      auto tmp = meshletData.size32();
      for (uint32_t k = 0; k < 3; k++) {
        auto vIx = indices[iIn + k];
        uint64_t key = vIx + 1;
        uint64_t val = 0;
        if (!localVertices.get(val, key)) {
          val = (meshletData.size32() - offset);
          meshletData.pushBack(vIx);
          localVertices.insert(key, val);
        }
        localindices[meshletIndices++] = uint8_t(val);
      }

      if (64 < (meshletData.size32() - offset) || 126 * 3 < meshletIndices) {
        meshletData.resize(tmp);
        meshletIndices -= 3;

        storeMeshlet(meshletData, meshlets, localindices, offset, meshletIndices);

        localVertices.clear();
        meshletIndices = 0;
        offset = meshletData.size32();
      }
      else {
        iIn += 3;
      }
    }
    if(meshletIndices)
      storeMeshlet(meshletData, meshlets, localindices, offset, meshletIndices);

    auto N = meshlets.size32();
    auto M = ((N + 31) & ~31u) - N; // pad up to 32
    for (uint32_t i = 0; i < M; i++) {
      meshlets.pushBack(Meshlet{ 0 });
    }
    assert((meshlets.size32() % 32) == 0);
  }

}

RenderSolidMS::RenderSolidMS(Logger logger, App* app) :
  logger(logger),
  app(app),
  textureManager(new RenderTextureManager(app->vCtx))
{

}

void RenderSolidMS::init()
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  taskShader = resources->createShader(vanilla_task, sizeof(vanilla_task));
  meshShader = resources->createShader(vanilla_mesh, sizeof(vanilla_mesh));
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

RenderSolidMS::~RenderSolidMS()
{
}

void RenderSolidMS::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto * frameManager = vCtx->frameManager;
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

      auto triangleCount = mesh->triCount;
      meshData.vtx = resources->createStorageBuffer(sizeof(Vertex) * 3 * triangleCount);

      Vector<uint32_t> indices;
      Vector<Vertex> vtx(3 * triangleCount);

      if (mesh->nrmCount) {
        if (false && mesh->texCount) {
          for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
            vtx[i] = Vertex(mesh->vtx[mesh->triVtxIx[i]],
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
            vtx[i] = Vertex(mesh->vtx[mesh->triVtxIx[ix]],
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
              vtx[3 * i + k] = Vertex(p[k],
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
              vtx[3 * i + k] = Vertex(p[k],
                                      n,
                                      Vec2f(0.5f),
                                      mesh->currentColor[i]);
            }
          }
        }
      }

      if (indices.empty()) {
        indices.resize(3 * triangleCount);
        for (uint32_t i = 0; i < indices.size32(); i++) indices[i] = i;
      }

      {
        Vector<uint32_t> reindices(indices.size());
        float fifo4, fifo8, fifo16, fifo32;
        getAverageCacheMissRatioPerTriangle(fifo4, fifo8, fifo16, fifo32, indices.data(), indices.size32());
        logger(0, "IN  AMCR FIFO4=%.2f, FIFO8=%.2f, FIFO16=%.2f, FIFO32=%.2f", fifo4, fifo8, fifo16, fifo32);
        linearSpeedVertexCacheOptimisation(logger, reindices.data(), indices.data(), indices.size32());
        getAverageCacheMissRatioPerTriangle(fifo4, fifo8, fifo16, fifo32, reindices.data(), indices.size32());
        logger(0, "OPT AMCR FIFO4=%.2f, FIFO8=%.2f, FIFO16=%.2f, FIFO32=%.2f", fifo4, fifo8, fifo16, fifo32);
        indices.swap(reindices);
      }

      Vector<uint32_t> meshletData;
      Vector<Meshlet> meshlets;
      Vector<Vec3f> P;

      buildMeshlets(meshletData, meshlets, indices);
      for (auto & meshlet : meshlets) {
        if (meshlet.vertexCount) {
          P.resize(meshlet.vertexCount);
          for (uint32_t i = 0; i < meshlet.vertexCount; i++) {
            auto k = meshletData[meshlet.offset + i];
            P[i] = Vec3f(vtx[k].px, vtx[k].py, vtx[k].pz);
          }

          Vec3f c0;
          float r0;
          boundingSphereNaive(c0, r0, P.data(), P.size());

          Vec3f c1;
          float r1;
          boundingSphere(c1, r1, P.data(), P.size());


          logger(0, "%.3f p=[%.2f %.2f %.2f] r=%.2f  p=[%.2f %.2f %.2f] r=%.2f", r1/r0, c0.x, c0.y, c0.z, r0, c1.x, c1.y, c1.z, r1);

        }

      }
      


      meshData.meshletData = resources->createStorageBuffer(meshletData.byteSize());
      meshData.meshlets = resources->createStorageBuffer(meshlets.byteSize());
      meshData.meshletCount = meshlets.size32();

      frameManager->stageAndCopyBuffer(meshData.vtx, vtx.data(), vtx.byteSize());
      frameManager->stageAndCopyBuffer(meshData.meshletData, meshletData.data(), meshletData.byteSize());
      frameManager->stageAndCopyBuffer(meshData.meshlets, meshlets.data(), meshlets.byteSize());

      logger(0, "%d meshlets (%d tris per meshlet), %.1f uints per meshlet",
             meshlets.size(), indices.size() / (3 * meshlets.size()), float(meshletData.size()) / meshlets.size());


      logger(0, "RenderSolidMS: Updated MeshData item");
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


void RenderSolidMS::buildPipelines(RenderPassHandle pass)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  VkDescriptorSetLayoutBinding objBufLayoutBinding[4];
  for (size_t i = 0; i < ARRAYSIZE(objBufLayoutBinding); i++) {
    objBufLayoutBinding[i] = {};
    objBufLayoutBinding[i].descriptorCount = 1;
  }
  objBufLayoutBinding[0].binding = 0;
  objBufLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  objBufLayoutBinding[0].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
  objBufLayoutBinding[1].binding = 1;
  objBufLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  objBufLayoutBinding[1].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
  objBufLayoutBinding[2].binding = 3;
  objBufLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  objBufLayoutBinding[2].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
  objBufLayoutBinding[3].binding = 4;
  objBufLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  objBufLayoutBinding[3].stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;

  VkDescriptorSetLayoutCreateInfo objBufLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  objBufLayoutInfo.bindingCount = ARRAYSIZE(objBufLayoutBinding);
  objBufLayoutInfo.pBindings = objBufLayoutBinding;

  VkDescriptorSetLayoutBinding objBufSamplerLayoutBinding[5];
  objBufSamplerLayoutBinding[0] = objBufLayoutBinding[0];
  objBufSamplerLayoutBinding[1] = objBufLayoutBinding[1];
  objBufSamplerLayoutBinding[2] = {};
  objBufSamplerLayoutBinding[2].binding = 2;
  objBufSamplerLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  objBufSamplerLayoutBinding[2].descriptorCount = 1;
  objBufSamplerLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  objBufSamplerLayoutBinding[3] = objBufLayoutBinding[2];
  objBufSamplerLayoutBinding[4] = objBufLayoutBinding[3];

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
                                              { taskShader, meshShader, solidShader },
                                              cullBackDepthBiasRasInfo);
  texturedPipeline = resources->createPipeline({ }, { },
                                               pipeLayoutInfo,
                                               objBufSamplerLayoutInfo,
                                               pass,
                                               { taskShader, meshShader, texturedShader },
                                               cullBackDepthBiasRasInfo);
}


void RenderSolidMS::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
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

      VkWriteDescriptorSet writes[4];
      for (size_t i = 0; i < ARRAYSIZE(writes); i++) {
        writes[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        writes[i].dstSet = set;
        writes[i].descriptorCount = 1;
      }
      writes[0].dstBinding = 0;
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writes[0].pBufferInfo = &objectBufferInfo;

      writes[1].dstBinding = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[1].pBufferInfo = &item.vtx.resource->descInfo;

      writes[2].dstBinding = 3;
      writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[2].pBufferInfo = &item.meshletData.resource->descInfo;

      writes[3].dstBinding = 4;
      writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[3].pBufferInfo = &item.meshlets.resource->descInfo;

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

    vCtx->vkCmdDrawMeshTasksNV(cmdBuf, (item.meshletCount + 31) / 32, 0);
  }
}

