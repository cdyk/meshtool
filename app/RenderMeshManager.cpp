#include "Mesh.h"
#include "RenderMeshManager.h"
#include "VulkanResources.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"

namespace {

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

RenderMeshManager::RenderMeshManager(VulkanContext* vCtx, Logger logger) :
  vCtx(vCtx),
  logger(logger)
{

}


RenderMeshHandle RenderMeshManager::createRenderMesh(Mesh* mesh)
{

  auto renderMeshHandle = createResource();
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

          for (unsigned i = 0; i < mesh->triCount; i++) {
            Vec3f u, v;
            tangentSpaceBasis(u, v,
                              mesh->vtx[mesh->triVtxIx[3 * i + 0]], mesh->vtx[mesh->triVtxIx[3 * i + 1]], mesh->vtx[mesh->triVtxIx[3 * i + 2]],
                              mesh->tex[mesh->triTexIx[3 * i + 0]], mesh->tex[mesh->triTexIx[3 * i + 1]], mesh->tex[mesh->triTexIx[3 * i + 2]]);

            for (unsigned k = 0; k < 3; k++) {
              auto n = normalize(mesh->nrm[mesh->triNrmIx[i]]);

              auto uu = normalize(u - dot(u, n)*n);
              auto vv = normalize(v - dot(v, n)*n);

              tanMap.mem[3 * i + k] = uu;
              bnmMap.mem[3 * i + k] = vv;
            }
          }

          for (unsigned i = 0; i < 3 * mesh->triCount; i++) {
            vtxMap.mem[i] = mesh->vtx[mesh->triVtxIx[i]];
            nrmMap.mem[i] = mesh->nrm[mesh->triNrmIx[i]];
            texMap.mem[i] = 10.f*mesh->tex[mesh->triTexIx[i]];
            //tanMap.mem[i] = Vec3f(0.f);
            //bnmMap.mem[i] = Vec3f(0.f);
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

  if (mesh->lineCount) {
    renderMesh->lineCount = mesh->lineCount;
    renderMesh->lines = vCtx->resources->createVertexDeviceBuffer(sizeof(Vec3fRGBA8) * 2 * renderMesh->lineCount);
    auto lineVtxStaging = vCtx->resources->createStagingBuffer(renderMesh->lines.resource->requestedSize);
    {
      MappedBuffer<Vec3fRGBA8> map(vCtx, lineVtxStaging);
      for (unsigned i = 0; i < 2 * renderMesh->lineCount; i++) {
        auto c = mesh->lineColor[i / 2];
        map.mem[i].p = mesh->vtx[mesh->lineVtxIx[i]];
        map.mem[i].r = (c >> 16) & 0xffu;
        map.mem[i].g = (c >> 8) & 0xffu;
        map.mem[i].b = (c >> 0) & 0xffu;
        map.mem[i].a = 255u;
      }
    }
    vCtx->frameManager->copyBuffer(renderMesh->lines, lineVtxStaging, renderMesh->lines.resource->requestedSize);
  }

  updateRenderMeshColor(renderMesh);

  logger(0, "CreateRenderMesh");
  return renderMeshHandle;
}

void RenderMeshManager::startFrame()
{
  Vector<RenderMeshResource*> orphans;
  getOrphans(orphans);
  for (auto * r : orphans) {
    delete r;
  }
}

void RenderMeshManager::updateRenderMeshColor(RenderMeshHandle renderMesh)
{
  auto * rm = renderMesh.resource;
  auto stagingBuf = vCtx->resources->createStagingBuffer(rm->col.resource->requestedSize);
  {
    MappedBuffer<RGBA8> map(vCtx, stagingBuf);
    for (unsigned i = 0; i < rm->mesh->triCount; i++) {
      auto color = rm->mesh->currentColor[i];

      RGBA8 col;
      col.r = (color >> 16) & 0xff;
      col.g = (color >> 8) & 0xff;
      col.b = (color) & 0xff;
      col.a = 255;

      map.mem[3 * i + 0] = col;
      map.mem[3 * i + 1] = col;
      map.mem[3 * i + 2] = col;
    }
  }
  vCtx->frameManager->copyBuffer(rm->col, stagingBuf, rm->col.resource->requestedSize);
  logger(0, "Updated mesh color");
}
