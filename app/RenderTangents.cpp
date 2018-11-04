#include <cassert>
#include "Common.h"
#include "App.h"
#include "RenderTangents.h"
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

  uint32_t coordSysVS[]
#include "coordSysVS.glsl.h"
    ;

  uint32_t flatPS[]
#include "flatPS.glsl.h"
    ;

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

RenderTangents::RenderTangents(Logger logger, App* app) :
  logger(logger),
  app(app)
{

}

void RenderTangents::init()
{
  auto * vCtx = app->vCtx;

  Vector<ShaderInputSpec> stages(2);
  stages[0] = { coordSysVS, sizeof(coordSysVS), VK_SHADER_STAGE_VERTEX_BIT };
  stages[1] = { flatPS, sizeof(flatPS), VK_SHADER_STAGE_FRAGMENT_BIT };
  shader = vCtx->resources->createShader(stages);


  renaming.resize(10);
  for (size_t i = 0; i < renaming.size(); i++) {
    renaming[i].ready = vCtx->resources->createFence(true);
    renaming[i].objectBuffer = vCtx->resources->createUniformBuffer(sizeof(ObjectBuffer));
  }
  
  coordSysVtxCol = vCtx->resources->createVertexDeviceBuffer(2*sizeof(Vec3f) * 6);
  auto coordSysStaging = vCtx->resources->createStagingBuffer(coordSysVtxCol.resource->requestedSize);
  {
    MappedBuffer<Vec3f> vtxMap(vCtx, coordSysStaging);
    vtxMap.mem[0] = Vec3f(0, 0, 0); vtxMap.mem[1] = Vec3f(1, 0, 0);
    vtxMap.mem[2] = Vec3f(1, 0, 0); vtxMap.mem[3] = Vec3f(1, 0, 0);
    vtxMap.mem[4] = Vec3f(0, 0, 0); vtxMap.mem[5] = Vec3f(0, 1, 0);
    vtxMap.mem[6] = Vec3f(0, 1, 0); vtxMap.mem[7] = Vec3f(0, 1, 0);
    vtxMap.mem[8] = Vec3f(0, 0, 0); vtxMap.mem[9] = Vec3f(0.3f, 0.3f, 1);
    vtxMap.mem[10] = Vec3f(0, 0, 1); vtxMap.mem[11] = Vec3f(0.3f, 0.3f, 1);
  }
  vCtx->frameManager->copyBuffer(coordSysVtxCol, coordSysStaging, coordSysVtxCol.resource->requestedSize);
}

RenderTangents::~RenderTangents()
{
}

void RenderTangents::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  newMeshData.clear();
  for (auto & mesh : meshes) {
    if (!(mesh->vtx && mesh->tex)) continue;

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
      logger(0, "Created new RenderTangents.MeshData item.");
    }
    auto & meshData = newMeshData.back();
    if (meshData.geometryGeneration != mesh->geometryGeneration) {
      meshData.geometryGeneration = mesh->geometryGeneration;

      meshData.triangleCount = mesh->triCount;
      meshData.vtx = resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * meshData.triangleCount);
      meshData.tan = resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * meshData.triangleCount);
      meshData.bnm = resources->createVertexDeviceBuffer(sizeof(Vec3f) * 3 * meshData.triangleCount);

      // FIXME: Should get unique triplets and average. And do a de-duplication wrt positions

      auto vtxStaging = resources->createStagingBuffer(meshData.vtx.resource->requestedSize);
      auto tanStaging = resources->createStagingBuffer(meshData.tan.resource->requestedSize);
      auto bnmStaging = resources->createStagingBuffer(meshData.bnm.resource->requestedSize);
      {
        MappedBuffer<Vec3f> vtxMap(vCtx, vtxStaging);
        MappedBuffer<Vec3f> tanMap(vCtx, tanStaging);
        MappedBuffer<Vec3f> bnmMap(vCtx, bnmStaging);

        if (mesh->nrm) {
          for (unsigned i = 0; i < mesh->triCount; i++) {
            Vec3f p[3];
            Vec2f t[3];
            for (unsigned k = 0; k < 3; k++) {
              p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
              t[k] = mesh->tex[mesh->triTexIx[3 * i + k]];
            }
            Vec3f u, v;
            tangentSpaceBasis(u, v, p[0], p[1], p[2], t[0], t[1], t[2]);
            for (unsigned k = 0; k < 3; k++) {
              auto n = normalize(mesh->nrm[mesh->triNrmIx[3 * i + k]]);
              auto uu = normalize(u - dot(u, n)*n);
              auto vv = normalize(v - dot(v, n)*n);
              vtxMap.mem[3 * i + k] = p[k];
              tanMap.mem[3 * i + k] = uu;
              bnmMap.mem[3 * i + k] = vv;
            }
          }
        }
        else {
          for (unsigned i = 0; i < mesh->triCount; i++) {
            Vec3f p[3];
            Vec2f t[3];
            for (unsigned k = 0; k < 3; k++) {
              p[k] = mesh->vtx[mesh->triVtxIx[3 * i + k]];
              t[k] = mesh->tex[mesh->triTexIx[3 * i + k]];
            }
            Vec3f u, v;
            tangentSpaceBasis(u, v, p[0], p[1], p[2], t[0], t[1], t[2]);
            auto n = normalize(cross(p[1] - p[0], p[2] - p[0]));
            auto uu = normalize(u - dot(u, n)*n);
            auto vv = normalize(v - dot(v, n)*n);
            for (unsigned k = 0; k < 3; k++) {
              vtxMap.mem[3 * i + k] = p[k];
              tanMap.mem[3 * i + k] = uu;
              bnmMap.mem[3 * i + k] = vv;
            }
          }
        }
      }
      vCtx->frameManager->copyBuffer(meshData.vtx, vtxStaging, meshData.vtx.resource->requestedSize);
      vCtx->frameManager->copyBuffer(meshData.tan, tanStaging, meshData.tan.resource->requestedSize);
      vCtx->frameManager->copyBuffer(meshData.bnm, bnmStaging, meshData.bnm.resource->requestedSize);
      logger(0, "Updated RenderTangents.MeshData item.");
    }
  }
  meshData.swap(newMeshData);
  for (auto & item : newMeshData) {
    if (item.src == nullptr) continue;
    // just handles and those will destroy themselves.
    logger(0, "Destroyed RenderTangents.MeshData item.");
  }
}


void RenderTangents::buildPipelines(RenderPassHandle pass)
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
    Vector<VkVertexInputAttributeDescription> inputAttrib(5);
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
    Vector<VkVertexInputBindingDescription> inputBind(4);
    inputBind[0] = { 0 };
    inputBind[0].stride = sizeof(Vec3f) + sizeof(Vec3f);
    inputBind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    for (unsigned i = 1; i < inputBind.size(); i++) {
      inputBind[i] = { 0 };
      inputBind[i].binding = i;
      inputBind[i].stride = sizeof(Vec3f);
      inputBind[i].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    }
    pipeline = resources->createPipeline(inputBind,
                                         inputAttrib,
                                         pipeLayoutInfo,
                                         objBufLayoutInfo,
                                         pass,
                                         shader,
                                         cullNothingRasInfo,
                                         VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    logger(0, "Built RenderTangents pipeline.");
  }

  for(auto & rename : renaming) {
    rename.objBufDescSet = resources->createDescriptorSet(pipeline.resource->descLayout);

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
    vkUpdateDescriptorSets(device, 1, writes, 0, nullptr);
  }

}

void RenderTangents::draw(VkCommandBuffer cmdBuf, RenderPassHandle pass, const Vec4f& viewport, const Mat3f& N, const Mat4f& MVP)
{
  auto * vCtx = app->vCtx;

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
    

    VkBuffer buffers[4] = {
      coordSysVtxCol.resource->buffer,
      item.vtx.resource->buffer,
      item.tan.resource->buffer,
      item.bnm.resource->buffer,
    };
    VkDeviceSize offsets[4] = { 0, 0, 0, 0 };
    vkCmdBindVertexBuffers(cmdBuf, 0, 4, buffers, offsets);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.resource->pipeLayout, 0, 1, &rename.objBufDescSet.resource->descSet, 0, NULL);
    vkCmdDraw(cmdBuf, 6, 3 * item.triangleCount, 0, 0);
   
  }
}

