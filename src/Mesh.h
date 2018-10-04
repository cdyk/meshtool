#pragma once
#include <cstdint>
#include "LinAlg.h"
#include "Common.h"

struct Mesh
{
  Arena arena;
  StringInterning strings;

  BBox3f bbox;
  Vec3f* vtx = nullptr;
  uint32_t vtx_n = 0;

  uint32_t* tri = nullptr;
  uint32_t tri_n = 0;

  const char** obj = nullptr;
  uint32_t obj_n = 0;

  const char* name = nullptr;
};
