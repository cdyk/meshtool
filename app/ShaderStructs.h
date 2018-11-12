#pragma once
#include <cinttypes>
#include "LinAlgOps.h"
#include "Half.h"

struct Vertex
{
  float px, py, pz;
  uint8_t nx, ny, nz, pad;
  uint16_t tu, tv;
  uint8_t r, g, b, a;


  Vertex(const Vec3f& p,
         const Vec3f& n,
         const Vec2f& t,
         const uint32_t c)
  {
    px = p.x;
    py = p.y;
    pz = p.z;
    auto nn = normalize(n);
    nx = uint8_t(127.f*nn.x + 127.f);
    ny = uint8_t(127.f*nn.y + 127.f);
    nz = uint8_t(127.f*nn.z + 127.f);
    tu = halfFromFloat(t.x);
    tv = halfFromFloat(t.y);
    r = (c >> 16) & 0xff;
    g = (c >> 8) & 0xff;
    b = (c) & 0xff;
    a = 255;
  }

};
static_assert(sizeof(Vertex) == 3 * 8);


struct ObjectBuffer
{
  Mat4f MVP;
  Vec4f Ncol0;
  Vec4f Ncol1;
  Vec4f Ncol2;
};
