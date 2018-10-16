#include "HalfEdgeMesh.h"
#include <algorithm>


HalfEdge::Vertex* HalfEdge::Mesh::createVertex(HalfEdge* leading)
{
  auto * v = store->allocVertex();
  v->leadingEdge = leading;
  vertices.pushBack(v);
  return v;
}

HalfEdge::HalfEdge* HalfEdge::Mesh::createHalfEdge(Vertex* srcVtx, HalfEdge* nextInFace, HalfEdge* nextInEdge, Face* face)
{
  auto * he = store->allocHalfEdge();
  he->srcVtx = srcVtx;
  if (srcVtx->leadingEdge == nullptr) {
    srcVtx->leadingEdge = he;
  }
  he->nextInFace = nextInFace;
  he->nextInEdge = nextInEdge;
  he->face = face;
  if (face->leadingEdge == nullptr) {
    face->leadingEdge = he;
  }

  halfEdges.pushBack(he);
  return he;
}

HalfEdge::Face* HalfEdge::Mesh::createFace(HalfEdge* leading)
{
  auto * f = store->allocFace();
  f->leadingEdge = leading;
  faces.pushBack(f);
  return f;
}

void HalfEdge::Mesh::insert(uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount)
{

  auto vtxMark = vertices.size32();
  for (uint32_t i = 0; i < vtxCount; i++) {
    createVertex();
  }

  auto mark = halfEdges.size32();

  Vector<HalfEdge*> loop;
  for (uint32_t f = 0; f < faceCount; f++) {
    auto * face = createFace();

    auto N = offsets[f + 1] - offsets[f];
    if (N < 3) {
      logger(1, "Degenerate face with less than 3 vertices.");
    }
    loop.resize(N);
    for (uint32_t i = 0; i < N; i++) {
      auto ix = indices[offsets[f] + i];
      loop[i] = createHalfEdge(vertices[vtxMark + ix], nullptr, nullptr, face);
      if (0 < i) {
        loop[i - 1]->nextInFace = loop[i];
      }
    }
    loop[N - 1]->nextInFace = loop[0];
  }

  stitchRange(mark, halfEdges.size32());
}

namespace {

  struct SortHelper
  {
    HalfEdge::HalfEdge* he;
    HalfEdge::Vertex* maj;
    HalfEdge::Vertex* min;
  };


}

void HalfEdge::Mesh::stitchRange(uint32_t heA, uint32_t heB)
{
  auto N = heB - heA;
  Vector<SortHelper> helper(N);
  for (uint32_t i = 0; i < N; i++) {
    auto & item = helper[i];
    item.he = halfEdges[heA + i];
    item.maj = item.he->srcVtx;
    item.min = item.he->nextInFace->srcVtx;
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

  for (uint32_t j = 0; j < N; ) {

    uint32_t i=j+1;
    for (; i < N && helper[j].maj == helper[i].maj && helper[j].min == helper[i].min; i++) ;

    auto abutting = i - j;
    if (abutting == 1) {
      boundaryEdges++;
      // stitch
    }
    else if (abutting == 2) {
      manifoldEdges++;
      // stitch
    }
    else {
      nonManifoldEdges++;
      // stitch
    }

    j = i;
  }

  logger(0, "Found %d boundary edges, %d manifold edges and %d non-manifold edges", boundaryEdges, manifoldEdges, nonManifoldEdges);
}



void HalfEdge::R3Mesh::insert(const Vec3f* vtx, uint32_t vtxCount, uint32_t* indices, uint32_t* offsets, uint32_t faceCount)
{
  auto vtxMark = vertices.size32();
  Mesh::insert(vtxCount, indices, offsets, faceCount);
  for (uint32_t i = 0; i < vtxCount; i++) {
    ((R3Vertex*)vertices[vtxMark + i])->pos = vtx[i];
  }
}
