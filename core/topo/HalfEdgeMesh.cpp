#include "HalfEdgeMesh.h"
#include <algorithm>

HalfEdgeMesh::VtxIx HalfEdgeMesh::createVertex(const Vec3f& p)
{
  auto * v = vertexStore.alloc();
  //v->leadingEdge = ~0u;
  v->pos = p;
  auto ix = vertexStore.getIndex(v);
  vertices.pushBack(ix);
  return ix;
}

HalfEdgeMesh::HEdgeIx HalfEdgeMesh::createHalfEdge(VtxIx srcVtx, HEdgeIx nextInFace, HEdgeIx nextInEdge, FaceIx face)
{
  auto * he = halfEdgeStore.alloc();
  he->srcVtx = srcVtx;
  he->nextInFace = nextInFace;
  he->nextInEdge = nextInEdge;
  he->face = face;
  auto ix = halfEdgeStore.getIndex(he);
  halfEdges.pushBack(ix);
  return ix;
}

HalfEdgeMesh::FaceIx HalfEdgeMesh::createFace()
{
  auto f = faceStore.alloc();
  auto ix = faceStore.getIndex(f);
  faces.pushBack(ix);
  return ix;
}


void HalfEdgeMesh::insert(const Vec3f* vtx, uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount)
{
  Vector<uint32_t> V;
  V.resize(vtxCount);

  for (uint32_t i = 0; i < vtxCount; i++) {
    V[i] = createVertex(vtx[i]);
  }

  auto mark = halfEdges.size32();
  Vector<uint32_t> HE;
  for (uint32_t f = 0; f < faceCount; f++) {
    uint32_t face = createFace();

    auto N = offsets[f + 1] - offsets[f];
    if (N < 3) {
      logger(1, "Degenerate face with less than 3 vertices.");
    }
    HE.resize(N);
    for (unsigned i = 0; i < N; i++) {
      HE[i] = createHalfEdge(V[indices[offsets[f] + i]], ~0u, ~0u, face);
      if (0 < i) {
        getHalfEdge(HE[i - 1])->nextInFace = HE[i];
      }
    }
    getHalfEdge(HE[N - 1])->nextInFace = HE[0];
  }

  stitchRange(mark, halfEdges.size32());
}

namespace {

  struct SortHelper
  {
    HalfEdgeMesh::HalfEdge* he;
    uint32_t maj;
    uint32_t min;
  };


}

void HalfEdgeMesh::stitchRange(uint32_t heA, uint32_t heB)
{
  auto N = heB - heA;
  Vector<SortHelper> helper(N);
  for (uint32_t i = 0; i < N; i++) {
    auto & item = helper[i];
    item.he = getHalfEdge(halfEdges[heA + i]);
    item.maj = item.he->srcVtx;
    item.min = getHalfEdge(item.he->nextInFace)->srcVtx;
    if (item.maj < item.min) {
      auto t = item.maj;
      item.maj = item.min;
      item.min = t;
    }
  }
  // A bit lazy, but will wait until it is a problem to do something more clever.
  std::sort(helper.begin(), helper.end(), [](auto& a, auto& b)
  {
    if (a.maj == b.maj) {
      return a.min < b.min;
    }
    return a.maj < b.maj;
  });

  uint32_t boundary = 0;
  uint32_t manifold = 0;
  uint32_t nonmanifold = 0;

  for (uint32_t j = 0; j < N; ) {

    uint32_t i=j+1;
    for (; i < N && helper[j].maj == helper[i].maj && helper[j].min == helper[i].min; i++) ;

    auto abutting = i - j;
    if (abutting == 1) {
      boundary++;
      // stitch
    }
    else if (abutting == 2) {
      manifold++;
      // stitch
    }
    else {
      nonmanifold++;
      // stitch
    }

    j = i;
  }

  logger(0, "Found %d boundary edges, %d manifold edges and %d non-manifold edges", boundary, manifold, nonmanifold);
}
