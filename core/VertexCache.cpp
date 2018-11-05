#include "VertexCache.h"
#include "Common.h"

namespace {

  template<uint32_t M>
  struct Cache
  {
    uint32_t entries[M];
    uint32_t misses = 0;
    uint32_t p = 0;

    Cache() { for (auto & e : entries) e = ~0u; }

    void lookup(uint32_t ix)
    {
      for (auto & e : entries) {
        if (e == ix) return;  // hit
      }
      misses++;
      entries[p] = ix;
      p = (p + 1) % M;
    }
  };

}

void getAverageCacheMissRatioPerTriangle(float& fifo4, float& fifo8, float& fifo16, float& fifo32, const uint32_t* indices, const uint32_t N)
{
  assert((N % 3) == 0);
  Cache<4> cache4;
  Cache<8> cache8;
  Cache<16> cache16;
  Cache<32> cache32;
  for (uint32_t i = 0; i < N; i++) {
    const auto ix = indices[i];
    cache4.lookup(ix);
    cache8.lookup(ix);
    cache16.lookup(ix);
    cache32.lookup(ix);
  }
  fifo4 = (3.f * cache4.misses) / float(N);
  fifo8 = (3.f * cache8.misses) / float(N);
  fifo16 = (3.f * cache16.misses) / float(N);
  fifo32 = (3.f * cache32.misses) / float(N);
}

struct LinSpdTri
{
  uint32_t next = ~0u;
  uint32_t ix[3];
};

struct LinSpdVtx
{
  uint32_t valence = 0;
};

void linearSpeedVertexCacheOptimisation(uint32_t * output, const uint32_t* input, const uint32_t N)
{
  assert((N % 3) == 0);

  const uint32_t cacheSize = 32;
  float cachePosScore[cacheSize] = { 0.75f, 0.75f, 0.75f };
  for (uint32_t i = 3; i < cacheSize; i++) {
    cachePosScore[i] = powf(1.0f - (i - 3) * (1.0f / (cacheSize - 3)), 1.5f);
  }

  const uint32_t maxValence = 8;
  float valenceScore[maxValence];
  for (uint32_t i = 0; i < maxValence; i++) {
    valenceScore[i] = powf(i + 1.f, -0.5f);
  }

  uint32_t maxIx = 0;
  for (auto * ix = input; ix != input + N; ix++) {
    maxIx = maxIx < *ix ? *ix : maxIx;
  }

  Vector<LinSpdVtx> vtx(maxIx);
  Vector<LinSpdTri> tri(N / 3);


  int a = 2;
}