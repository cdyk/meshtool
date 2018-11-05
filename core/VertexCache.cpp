#include "VertexCache.h"


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
