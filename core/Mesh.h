#pragma once
#include <cstdint>
#include "LinAlg.h"
#include "Common.h"

struct Mesh
{
  Arena arena;
  StringInterning strings;

  uint32_t geometryGeneration = 1;  // is never zero
  uint32_t colorGeneration = 1;     // is never zero

  BBox3f bbox;
  Vec3f* vtx = nullptr;
  uint32_t vtxCount = 0;

  Vec3f* nrm = nullptr;
  uint32_t nrmCount = 0;

  Vec2f* tex = nullptr;
  uint32_t texCount;

  uint32_t* triVtxIx = nullptr;
  uint32_t* triNrmIx = nullptr;
  uint32_t* triTexIx = nullptr;
  uint32_t* TriObjIx = nullptr;   // Triangle object index, one uint32_t per triangle.
  uint32_t* triColor = nullptr;
  uint32_t* triSmoothGroupIx = nullptr;
  uint32_t triCount = 0;

  uint32_t* lineVtxIx = nullptr;
  uint32_t* lineColor = nullptr;
  uint32_t lineCount = 0;

  const char** obj = nullptr;
  uint32_t obj_n = 0;

  // move to mesh app state or something.
  uint32_t* currentColor = nullptr;  // Current triangle color, one uint32_t per triangle;
  bool * selected = nullptr;

  const char* name = nullptr;

  void touchColor() { geometryGeneration++; if (!geometryGeneration) geometryGeneration++; }
  void touchGeometry() { geometryGeneration++; if (!geometryGeneration) geometryGeneration++; touchColor(); }
};
