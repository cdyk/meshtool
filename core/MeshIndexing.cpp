#include "MeshIndexing.h"


void getEdges(Logger logger, Vector<uint32_t>& edgeIndices, const uint32_t* triVtxIx, const uint32_t triCount)
{
  edgeIndices.clear();

  Map known;
  for (uint32_t j = 0; j < 3 * triCount; j += 3) {
    for (uint32_t i = 0; i < 3; i++) {
      auto a = triVtxIx[j + i];
      auto b = triVtxIx[j + i + (i < 2 ? 1 : -2)];
      if (b < a) {
        auto t = a;
        a = b;
        b = t;
      }
      auto key = (uint64_t(a) << 32u) | uint64_t(b + 1);
      if (known.get(key) == 0) {
        edgeIndices.pushBack(a);
        edgeIndices.pushBack(b);
        known.insert(key, 1);
      }
    }
  }
  logger(0, "getEdges: %d triangles with %d edges.", triCount, edgeIndices.size()/2);
}

void uniqueIndices(Logger logger, Vector<uint32_t>& indices, Vector<uint32_t>& vertices, const uint32_t* vtxIx, const uint32_t* nrmIx, const uint32_t N)
{
  Map known;
  vertices.clear();
  indices.resize(N);
  for (uint32_t i = 0; i < N; i++) {
    auto v = uint64_t(vtxIx[i] + 1);
    auto n = uint64_t(nrmIx[i]);
    auto key = (n << 32u) | v;
    uint64_t val = 0;
    if (!known.get(val, key)) {
      val = vertices.size();
      vertices.pushBack(i);
      known.insert(key, val);
    }
    indices[i] = uint32_t(val);
  }
  logger(0, "uniqueIndices: %d index pairs where %d were unique.", N, vertices.size());
}
