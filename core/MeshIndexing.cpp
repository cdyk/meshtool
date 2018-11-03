#include "MeshIndexing.h"

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
