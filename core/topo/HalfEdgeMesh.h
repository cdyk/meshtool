#pragma once
#include "../mem/Allocators.h"
#include "../Common.h"
#include "../LinAlg.h"

class HalfEdgeMesh : NonCopyable
{
public:
  HalfEdgeMesh(Logger logger) : logger(logger) {}

  struct Vertex {
    uint32_t leadingEdge = ~0u;
    Vec3f pos;
  };

  struct HalfEdge {
    uint32_t srcVtx = ~0u;
    uint32_t nextInFace = ~0u;
    uint32_t nextInEdge = ~0u;
    uint32_t face = ~0u;
  };

  struct Face {
    uint32_t leadingEdge = ~0u;
    uint32_t padding;
  };

  Vector<uint32_t> vertices;
  Vector<uint32_t> halfEdges;
  Vector<uint32_t> faces;

  void insert(const Vec3f* vtx, uint32_t* indices, uint32_t* offsets, uint32_t faceCount);

private:
  Logger logger;
  Pool<Vertex> vertexStore;
  Pool<HalfEdge> halfEdgeStore;
  Pool<Face> faceStore;
};