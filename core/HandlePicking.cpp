#include "HandlePicking.h"
#include "LinAlgOps.h"
//#include "App.h"  // MeshItem
#include "Mesh.h"

namespace {

  uint8_t insideFrustumPlaneMask(Vec4f& h)
  {
    return
      ( h.x <= h.w ? 32 : 0) |
      (-h.w <= h.x ? 16 : 0) |
      ( h.y <= h.w ?  8 : 0) |
      (-h.w <= h.y ?  4 : 0) |
      ( h.z <= h.w ?  2 : 0) |
      (-h.w <= h.z ?  1 : 0);
  }




}


bool nearestHit(Hit& hit,
                HitTmp& tmp,
                Logger logger,
                const Vector<Mesh*>& meshes,
                const Mat4f& PM,
                const Mat4f& PMinv,
                const Vec2f& viewPortPos,
                const Vec2f& viewPortSize,
                const Vec2f& screenPos)
{
  float nearestZ = FLT_MAX;
  auto & vmasks = tmp.vertexMasks;

  Vec2f pickExtent(5.f, 5.f);

  auto sx = (1.f / pickExtent.x) * viewPortSize.x;
  auto sy = (1.f / pickExtent.y) * viewPortSize.y;
  auto tx = (1.f / pickExtent.x) * (viewPortSize.x - 2.f*(screenPos.x - viewPortPos.x));
  auto ty = -(1.f / pickExtent.y) * (viewPortSize.y - 2.f*(screenPos.y - viewPortPos.y));

  Mat4f pickMatrix(sx, 0.f, 0.f, tx,
                   0.f, sy, 0.f, ty,
                   0.f, 0.f, 1.f, 0.f,
                   0.f, 0.f, 0.f, 1.f);

  Mat4f pickSpaceFromWorld = mul(pickMatrix, PM);

  for (unsigned j = 0; j < meshes.size32(); j++) {
    auto * m = meshes[j];

    auto mask = 0;
    for (unsigned i = 0; i < 8; i++) {
      auto h = mul(pickSpaceFromWorld, Vec4f(i & 1 ? m->bbox.min.x : m->bbox.max.x,
                                             i & 2 ? m->bbox.min.y : m->bbox.max.y,
                                             i & 4 ? m->bbox.min.z : m->bbox.max.z,
                                             1.f));
      mask = mask | insideFrustumPlaneMask(h);
    }
    if (mask != 63) {
      logger(0, "BBOX skip");
      continue;
    }

    vmasks.accommodate(m->vtxCount);
    for (unsigned i = 0; i < m->vtxCount; i++) {
      auto & p = m->vtx[i];
      auto h = mul(pickSpaceFromWorld, Vec4f(p.x, p.y, p.z, 1.f));
      vmasks[i] = insideFrustumPlaneMask(h);
    }

    for (unsigned i = 0; i < m->triCount; i++) {
      auto ix0 = m->triVtxIx[3 * i + 0];
      auto ix1 = m->triVtxIx[3 * i + 1];
      auto ix2 = m->triVtxIx[3 * i + 2];

      if ((vmasks[ix0] | vmasks[ix1] | vmasks[ix2]) != 63) {
        continue; // vertices are comletely outside at least one plane
      }

      auto h0 = mul(pickSpaceFromWorld, Vec4f(m->vtx[ix0], 1.f));
      auto h1 = mul(pickSpaceFromWorld, Vec4f(m->vtx[ix1], 1.f));
      auto h2 = mul(pickSpaceFromWorld, Vec4f(m->vtx[ix2], 1.f));
      if ((vmasks[ix0] & vmasks[ix1] & vmasks[ix2] & 1) == 1) {
        // all vertices in front of near plane

        auto p0 = (1.f / h0.w)*Vec2f(h0.data);
        auto p1 = (1.f / h1.w)*Vec2f(h1.data);
        auto p2 = (1.f / h2.w)*Vec2f(h2.data);
        auto v01 = p1 - p0;
        auto v12 = p2 - p1;
        auto v20 = p0 - p2;

        auto area12o = v12.x*p1.y - p1.x*v12.y;
        auto area20o = v20.x*p2.y - p2.x*v20.y;
        auto area01o = v01.x*p0.y - p0.x*v01.y;
        auto area012 = v01.x*v20.y - v20.x*v01.y;

        if (1e-5f < std::abs(area012)) {
          if ((signbit(area12o) == signbit(area20o)) &&
              (signbit(area01o) == signbit(area012)) &&
              (signbit(area20o) == signbit(area01o)))
          {
            auto s = 1.f / (area012);
            auto b0 = (1.f / h0.w) * s * area12o;
            auto b1 = (1.f / h1.w) * s * area20o;
            auto b2 = (1.f / h2.w) * s * area01o;

            auto z = b0 * h0.z + b1 * h1.z + b2 * h2.z;
            if (z < nearestZ) {
              nearestZ = z;
              hit.mesh = j;
              hit.triangle = i;
              hit.depth = nearestZ;
            }
          }
        }

        else {
          logger(0, "Degenerate");
        }
      }

      else {
        // Handle with clipping or do ray-intersection in world space.
        logger(0, "Triangle intersecting near plane");
      }
    }


  }
  return nearestZ < FLT_MAX;
}