#include <cassert>
#include "Common.h"
#include "App.h"
#include "RenderOutlines.h"
#include "Mesh.h"
#include "MeshIndexing.h"
#include "VulkanContext.h"
#include "RenderTextureManager.h"
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

RenderOutlines::RenderOutlines(Logger logger, App* app) :
  logger(logger),
  app(app)
{

}

void RenderOutlines::init()
{
  auto * vCtx = app->vCtx;

  {
    Vector<ShaderInputSpec> stages(2);
    stages[0] = { flatVS, sizeof(flatVS), VK_SHADER_STAGE_VERTEX_BIT };
    stages[1] = { flatPS, sizeof(flatPS), VK_SHADER_STAGE_FRAGMENT_BIT };
    flatShader = vCtx->resources->createShader(stages);
  }

  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->resources->createFence(true);
    renaming[i].objectBuffer = vCtx->resources->createUniformBuffer(sizeof(ObjectBuffer));
  }
}

RenderOutlines::~RenderOutlines()
{
}

void RenderOutlines::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;
  auto * frameManager = vCtx->frameManager;

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
      logger(0, "Created new RenderOutlines.MeshData item.");
    }
    auto & meshData = newMeshData.back();
    if (meshData.geometryGeneration != mesh->geometryGeneration) {
      meshData.geometryGeneration = mesh->geometryGeneration;
      meshData.colorGeneration = 0; // trigger update

      Vector<uint32_t> indices;
      getEdges(logger, indices, mesh->triVtxIx, mesh->triCount);

      meshData.outlineCount = indices.size32() / 2;
      meshData.lineOffset = mesh->vtxCount;
      meshData.lineCount = mesh->lineCount;
      meshData.vertexCount = mesh->vtxCount + 2 * meshData.lineCount;
      meshData.vtx = resources->createVertexDeviceBuffer(sizeof(Vec3f) * meshData.vertexCount);
      meshData.col = resources->createVertexDeviceBuffer(sizeof(uint32_t) * meshData.vertexCount);

      auto vtxStaging = resources->createStagingBuffer(meshData.vtx.resource->requestedSize);
      {
        MappedBuffer<Vec3f> map(vCtx, vtxStaging);
        std::memcpy(map.mem, mesh->vtx, sizeof(Vec3f)*mesh->vtxCount);
        for (uint32_t e = 0; e < 2 * meshData.lineCount; e++) {
          map.mem[meshData.lineOffset + e] = mesh->vtx[mesh->lineVtxIx[e]];
        }
      }
      frameManager->copyBuffer(meshData.vtx, vtxStaging, meshData.vtx.resource->requestedSize);

      meshData.indices = resources->createIndexDeviceBuffer(sizeof(uint32_t) * 2 * meshData.outlineCount);
      frameManager->stageAndCopyBuffer(meshData.indices, indices.data(), sizeof(uint32_t) * 2 * meshData.outlineCount);
      logger(0, "RenderOutlines: Updated geometry.");
    }

    if (meshData.colorGeneration != mesh->colorGeneration) {
      meshData.colorGeneration = mesh->colorGeneration;

      auto stagingBuf = vCtx->resources->createStagingBuffer(meshData.col.resource->requestedSize);
      {
        RGBA8 outlines;
        outlines.r = 0xff;
        outlines.g = 0xff;
        outlines.b = 0xff;
        outlines.a = 255;

        MappedBuffer<RGBA8> map(vCtx, stagingBuf);
        for (unsigned i = 0; i < meshData.lineOffset; i++) {
          map.mem[i] = outlines;
        }
        for (uint32_t e = 0; e < 2 * meshData.lineCount; e++) {
          auto c = meshData.src->lineColor[e / 2];
          map.mem[meshData.lineOffset + e].r = (c >> 16) & 0xffu;
          map.mem[meshData.lineOffset + e].g = (c >> 8) & 0xffu;
          map.mem[meshData.lineOffset + e].b = (c >> 0) & 0xffu;
          map.mem[meshData.lineOffset + e].a = 255u;
        }
      }
      vCtx->frameManager->copyBuffer(meshData.col, stagingBuf, meshData.col.resource->requestedSize);
      logger(0, "RenderOutlines: Updated colors.");
    }
  }
  meshData.swap(newMeshData);
  for (auto & item : newMeshData) {
    if (item.src == nullptr) continue;
    // just handles and those will destroy themselves.
    logger(0, "RenderOutlines: Destroyed meshdata item.");
  }
}


void RenderOutlines::buildPipelines(RenderPassHandle pass)
{
  auto * vCtx = app->vCtx;

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

 
  VkPipelineLayoutCreateInfo pipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pipeLayoutInfo.pushConstantRangeCount = 0;
  pipeLayoutInfo.pPushConstantRanges = NULL;

  VkPipelineRasterizationStateCreateInfo cullNothingRasInfo{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  cullNothingRasInfo.polygonMode = VK_POLYGON_MODE_FILL;
  cullNothingRasInfo.cullMode = VK_CULL_MODE_NONE;
  cullNothingRasInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  cullNothingRasInfo.depthClampEnable = VK_FALSE;
  cullNothingRasInfo.rasterizerDiscardEnable = VK_FALSE;
  cullNothingRasInfo.depthBiasEnable = VK_FALSE;
  cullNothingRasInfo.lineWidth = 1.0f;

  Vector<VkVertexInputAttributeDescription> inputAttrib(2);
  inputAttrib[0] = { 0 };
  inputAttrib[0].location = 0;
  inputAttrib[0].binding = 0;
  inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  inputAttrib[1] = { 0 };
  inputAttrib[1].location = 1;
  inputAttrib[1].binding = 1;
  inputAttrib[1].format = VK_FORMAT_R8G8B8A8_UNORM;

  Vector<VkVertexInputBindingDescription> inputBind(2);
  inputBind[0] = { 0 };
  inputBind[0].stride = sizeof(Vec3f);
  inputBind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  inputBind[1] = { 0 };
  inputBind[1].binding = 1;
  inputBind[1].stride = sizeof(RGBA8);
  inputBind[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  linePipeline = vCtx->resources->createPipeline(inputBind,
                                                 inputAttrib,
                                                 pipeLayoutInfo,
                                                 objBufLayoutInfo,
                                                 pass,
                                                 flatShader,
                                                 cullNothingRasInfo,
                                                 VK_PRIMITIVE_TOPOLOGY_LINE_LIST);

  for (size_t i = 0; i < renaming.size(); i++) {
    auto & rename = renaming[i];

    renaming[i].objBufDescSet = vCtx->resources->createDescriptorSet(linePipeline.resource->descLayout);

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

}

void RenderOutlines::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP, bool outlines, bool lines)
{
  auto * vCtx = app->vCtx;

  if (!linePipeline || linePipeline.resource->pass != pass) buildPipelines(pass);

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

    VkBuffer buffers[2] = { item.vtx.resource->buffer, item.col.resource->buffer };
    VkDeviceSize offsets[ARRAYSIZE(buffers)] = { 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, ARRAYSIZE(buffers), buffers, offsets);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);

    if (outlines) {
      vkCmdBindIndexBuffer(cmdBuf, item.indices.resource->buffer, 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(cmdBuf, 2 * item.outlineCount, 1, 0, 0, 0);
    }

    if (lines && item.lineCount) {
      vkCmdDraw(cmdBuf, 2 * item.lineCount, 1, item.lineOffset, 0);
    }
   
  }
}

