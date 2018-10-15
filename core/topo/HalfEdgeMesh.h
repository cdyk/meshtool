#pragma once
#include "../mem/Allocators.h"
#include "../Common.h"
#include "../LinAlg.h"

class HalfEdgeMesh : NonCopyable
{
public:
  typedef uint32_t VtxIx;
  typedef uint32_t HEdgeIx;
  typedef uint32_t FaceIx;

  HalfEdgeMesh(Logger logger) : logger(logger) {}

  struct Vertex {
    HEdgeIx leadingEdge = ~0u;
    Vec3f pos;
  };

  struct HalfEdge {
    VtxIx srcVtx = ~0u;
    HEdgeIx nextInFace = ~0u;
    HEdgeIx nextInEdge = ~0u;
    FaceIx face = ~0u;
  };

  struct Face {
    HEdgeIx leadingEdge = ~0u;
    uint32_t padding;
  };

  Vector<VtxIx> vertices;
  Vector<HEdgeIx> halfEdges;
  Vector<FaceIx> faces;

  VtxIx createVertex(const Vec3f& p);

  Vertex* getVertex(VtxIx ix) { return vertexStore.fromIndex(ix); }
  HalfEdge* getHalfEdge(HEdgeIx ix) { return halfEdgeStore.fromIndex(ix); }
  Face* getFace(FaceIx ix) { return faceStore.fromIndex(ix); }

  void insert(const Vec3f* vtx, uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount);

private:
  Logger logger;
  Pool<Vertex> vertexStore;
  Pool<HalfEdge> halfEdgeStore;
  Pool<Face> faceStore;

  HEdgeIx createHalfEdge(VtxIx srcVtx = ~0u, HEdgeIx nextInFace = ~0u, HEdgeIx nextInEdge = ~0u, FaceIx face = ~0u);
  FaceIx createFace();

  void stitchRange(uint32_t heA, uint32_t heB);

};