#include <cassert>
#include "Common.h"
#include "App.h"
#include "RenderNormals.h"
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
  float scale;
};

namespace {

  uint32_t coordSysVS[] = {
#include "normals.vert.h"
  };

  uint32_t flatPS[]
#include "flatPS.glsl.h"
    ;

  struct Vertex {
    Vec3f p;
    Vec3f n;
    uint32_t color;
    uint32_t dummy;
  };

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

RenderNormals::RenderNormals(Logger logger, App* app) :
  logger(logger),
  app(app)
{

}

void RenderNormals::init()
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  vertexShader = resources->createShader(coordSysVS, sizeof(coordSysVS));
  fragmentShader = resources->createShader(flatPS, sizeof(flatPS));

}

RenderNormals::~RenderNormals()
{
}

void RenderNormals::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  newMeshData.clear();
  for (auto & mesh : meshes) {
    if (!(mesh->vtx && mesh->nrm)) continue;

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
      logger(0, "Created new RenderNormals.MeshData item.");
    }
    auto & meshData = newMeshData.back();
    if (meshData.geometryGeneration != mesh->geometryGeneration || meshData.colorGeneration != mesh->colorGeneration) {
      meshData.geometryGeneration = mesh->geometryGeneration;
      meshData.colorGeneration = mesh->colorGeneration;

      meshData.vertexCount = 3*mesh->triCount;
      meshData.vtxNrm = resources->createVertexDeviceBuffer(sizeof(Vertex) * meshData.vertexCount);

      auto vtxNrmStaging = resources->createStagingBuffer(meshData.vtxNrm.resource->requestedSize);
      {
        auto * mem = (Vertex*)vtxNrmStaging.resource->hostPtr;
        for (unsigned t = 0; t < mesh->triCount; t++) {
          bool selected = mesh->selected[mesh->TriObjIx[t]];
          for (unsigned i = 0; i < 3; i++) {
            auto k = 3 * t + i;
            mem[k].p = mesh->vtx[mesh->triVtxIx[k]];
            mem[k].n = mesh->nrm[mesh->triNrmIx[k]];
            mem[k].color = selected ? 0xffff88 : 0xff4444;
          }
        }
      }
      vCtx->frameManager->copyBuffer(meshData.vtxNrm, vtxNrmStaging, meshData.vtxNrm.resource->requestedSize);
      logger(0, "Updated RenderNormals.MeshData item.");
    }
  }
  meshData.swap(newMeshData);
  for (auto & item : newMeshData) {
    if (item.src == nullptr) continue;
    // just handles and those will destroy themselves.
    logger(0, "Destroyed RenderNormals.MeshData item.");
  }
}


void RenderNormals::buildPipelines(RenderPassHandle pass)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;
  auto device = vCtx->device;

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
  {
    Vector<VkVertexInputAttributeDescription> inputAttrib(3);
    inputAttrib[0] = { 0 };
    inputAttrib[0].location = 0;
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[0].offset = offsetof(Vertex, Vertex::p);
    inputAttrib[1] = { 0 };
    inputAttrib[1].location = 1;
    inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].offset = offsetof(Vertex, Vertex::n);
    inputAttrib[2] = { 0 };
    inputAttrib[2].location = 2;
    inputAttrib[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    inputAttrib[2].offset = offsetof(Vertex, Vertex::color);

    VkVertexInputBindingDescription inputBind{ 0 };
    inputBind.stride = sizeof(Vertex);
    inputBind.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    pipeline = resources->createPipeline({ inputBind },
                                         inputAttrib,
                                         pipeLayoutInfo,
                                         objBufLayoutInfo,
                                         pass,
                                         {vertexShader, fragmentShader},
                                         cullNothingRasInfo,
                                         VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    logger(0, "Built RenderNormals pipeline.");
  }

}

void RenderNormals::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * vCtx = app->vCtx;
  auto * frameManager = vCtx->frameManager;

  if (!pipeline || pipeline.resource->pass != pass) buildPipelines(pass);

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
    
    VkDescriptorBufferInfo objectBufferInfo;
    auto* objectBuffer = (ObjectBuffer*)vCtx->frameManager->allocUniformStore(objectBufferInfo, sizeof(ObjectBuffer));
    objectBuffer->MVP = MVP;
    objectBuffer->Ncol0 = Vec4f(N.cols[0], 0.f);
    objectBuffer->Ncol1 = Vec4f(N.cols[1], 0.f);
    objectBuffer->Ncol2 = Vec4f(N.cols[2], 0.f);
    objectBuffer->scale = 0.01f;

    VkDescriptorSet set = frameManager->allocDescriptorSet(pipeline);

    VkWriteDescriptorSet writes[1];
    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[0].dstSet = set;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &objectBufferInfo;
    vkUpdateDescriptorSets(vCtx->device, 1, writes, 0, nullptr);

    VkBuffer buffers[1] = { item.vtxNrm.resource->buffer };
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, buffers, offsets);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.resource->pipeLayout, 0, 1, &set, 0, NULL);
    vkCmdDraw(cmdBuf, 2, item.vertexCount, 0, 0);
  
  }
}

