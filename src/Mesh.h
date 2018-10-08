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
  uint32_t vtxCount = 0;

  Vec3f* nrm = nullptr;
  uint32_t nrmCount = 0;

  uint32_t* triVtxIx = nullptr;
  uint32_t* triNrmIx = nullptr;
  uint32_t* TriObjIx = nullptr;   // Triangle object index, one uint32_t per triangle.
  uint32_t triCount = 0;


  const char** obj = nullptr;
  uint32_t obj_n = 0;


  // move to mesh app state or something.
  uint32_t* currentColor = nullptr;  // Current triangle color, one uint32_t per triangle;
  bool * selected = nullptr;

  const char* name = nullptr;
};
