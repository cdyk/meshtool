#pragma once
#include "Common.h"
#include "LinAlg.h"


struct Hit
{
  uint32_t mesh;
  uint32_t triangle;
  float depth;
};

struct HitTmp
{
  MemBuffer<uint8_t> vertexMasks;
};


bool nearestHit(Hit& hit,
                HitTmp& tmp,
                Logger logger,
                const Vector<Mesh*>& meshes,
                const Mat4f& PM,
                const Mat4f& PMinv,
                const Vec2f& viewPortPos,
                const Vec2f& viewPortSize,
                const Vec2f& screenPos);

