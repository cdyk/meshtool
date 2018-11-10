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


struct LinSpd
{

  struct Tri
  {
    float score = 0.f;
    uint32_t active = 1;
  };

  struct Vtx
  {
    float score = 0.f;
    uint32_t offset = 0;
    uint32_t activeTriangles = 0;
    uint32_t cachePos = ~0u;
  };

  Logger logger;

  static const uint32_t cacheSize = 32;
  float cachePosScore[cacheSize] = { 0.75f, 0.75f, 0.75f };

  uint32_t cache[cacheSize + 3];
  uint32_t cachePointer = 0;

  static const uint32_t maxValence = 16;
  float valenceScore[maxValence];


  uint32_t* output;
  const uint32_t* input;
  const uint32_t Nt;
  uint32_t emitted = 0;

  uint32_t maxFoundValence = 0;

  uint32_t maxIx = 0;
  Vector<Vtx> vtx;
  Vector<Tri> tri;

  Vector<uint32_t> vtxTri;

  LinSpd(Logger logger, uint32_t* output, const uint32_t* input, const uint32_t Nt) :
    logger(logger),
    output(output),
    input(input),
    Nt(Nt)
  {

  }

  
  void findMaxVertexIndex()
  {
    for (auto * ix = input; ix != input + 3 * Nt; ix++) {
      maxIx = maxIx < *ix ? *ix : maxIx;
    }
    maxIx++;
  }

  void buildVertexTriangleList()
  {
    for (uint32_t t = 0; t < Nt; t++) {
      for (uint32_t i = 0; i < 3; i++) {
        auto v = input[3 * t + i];
        vtx[v].activeTriangles++;
        if (maxFoundValence < vtx[v].activeTriangles) {
          maxFoundValence = vtx[v].activeTriangles;
        }
      }
    }
    uint32_t offset = 0;
    for (uint32_t v = 0; v < maxIx; v++) {
      vtx[v].offset = offset;

      offset += vtx[v].activeTriangles;

      vtx[v].activeTriangles = 0;
    }
    assert(offset == 3 * Nt);

    vtxTri.resize(3 * Nt);
    for (uint32_t t = 0; t < Nt; t++) {
      for (uint32_t i = 0; i < 3; i++) {
        auto v = input[3 * t + i];
        vtxTri[vtx[v].offset + vtx[v].activeTriangles] = t;
        vtx[v].activeTriangles++;
      }
    }
  }

  void removeTriangleFromVertexTriangleList(uint32_t t)
  {
    // detach triangle from vertices.
    for (uint32_t i = 0; i < 3; i++) {
      auto v = input[3 * t + i];

      bool found = false;
      assert(vtx[v].activeTriangles);

      // offsets into vtxtri and reduce number of active triangles.
      auto oa = vtx[v].offset;
      auto ob = oa + (--vtx[v].activeTriangles);
      for (uint32_t o = oa; o <= ob; o++) {

        // and let triangle at tail take t's place.
        if (vtxTri[o] == t) {
          vtxTri[o] = vtxTri[ob];
          found = true;
          break;
        }
      }
      assert(found);
    }
  }

  void updateVertexCache(uint32_t t)
  {
    for (uint32_t i = 0; i < 3; i++) {
      auto v = input[3 * t + i];

      // search for hit
      bool hit = false;
      for (auto & e : cache) {
        if (e == v) { hit = true; break; }
      }

      // if not cache hit
      if (!hit) {
        // push all items backwards, done by updating vertex's cache position
        for (auto & v : cache) {
          if (v == ~0) continue;
          assert(v < maxIx);
          vtx[v].cachePos++;
        }
        auto w = cache[cachePointer];
        if (w != ~0) {
          vtx[v].score = 0.f;
        }

        // insert v at front
        cache[cachePointer] = v;
        vtx[v].cachePos = 0;
        cachePointer = (cachePointer + 1) % uint32_t(ARRAYSIZE(cache));
      }
    }
  }
  
  uint32_t updateScoresAndFindBestCandidate()
  {
    // update score and find candidate
    for (auto & e : cache) {
      if (e == ~0u) continue;
      auto & V = vtx[e];
      float cScore = V.cachePos < cacheSize ? cachePosScore[V.cachePos] : 0.f;
      float vScore = valenceScore[V.activeTriangles < maxValence ? V.activeTriangles : maxValence];
      V.score = cScore * vScore;
    }

    uint32_t candidate = ~0u;
    float candidateScore = -1.f;
    for (auto & e : cache) {
      if (e == ~0u) continue;

      auto oa = vtx[e].offset;
      auto ob = oa + vtx[e].activeTriangles;

      for (uint32_t o = oa; o < ob; o++) {
        auto t = vtxTri[o];
        auto & T = tri[t];
        assert(T.active);
        T.score = vtx[input[3 * t + 0]].score +
          vtx[input[3 * t + 1]].score +
          vtx[input[3 * t + 2]].score;
        if (candidateScore < T.score) {
          candidateScore = T.score;
          candidate = t;
        }
      }
    }

    if (candidate == ~0u) {
      for (auto & e : cache) {
        if (e == ~0u) continue;
        assert(vtx[e].activeTriangles == 0);
      }
    }

    return candidate;
  }


  void run()
  {
    for (uint32_t i = 3; i < cacheSize; i++) {
      cachePosScore[i] = powf(1.0f - (i - 3) * (1.0f / (cacheSize - 3)), 1.5f);
    }

    valenceScore[0] = 0.f;
    for (uint32_t i = 1; i < maxValence; i++) {
      valenceScore[i] = 2.f*std::pow(float(i), -0.5f);
    }

    for (auto & e : cache) e = ~0u;

    findMaxVertexIndex();
    vtx.resize(maxIx);
    tri.resize(Nt);
    buildVertexTriangleList();


    uint32_t dry = 0;
    uint32_t candidate = 0;
    while (emitted < Nt) {

      if ((emitted % 100000) == 0) {
        logger(0, "Emitted %d", emitted);
        uint32_t g = 0;
        for (uint32_t t = 0; t < Nt; t++) {
          if (tri[t].active) g++;
        }
        assert(g == Nt - emitted);
      }

      if (candidate == ~0u) {
        dry++;
        for (uint32_t t = 0; t < Nt; t++) {
          if (tri[t].active) {
            candidate = t; break;
          }
        }
      }
      assert(candidate != ~0u);

      assert(tri[candidate].active);
      tri[candidate].active = 0;
      output[3 * emitted + 0] = input[3 * candidate + 0];
      output[3 * emitted + 1] = input[3 * candidate + 1];
      output[3 * emitted + 2] = input[3 * candidate + 2];
      emitted++;


      removeTriangleFromVertexTriangleList(candidate);
      updateVertexCache(candidate);

      candidate = updateScoresAndFindBestCandidate();
    }

    logger(0, "Nt=%d, maxIx=%d, maxValence=%d, dry=%.3f", Nt, maxIx, maxFoundValence, float(dry)/Nt);

    // sanity check : make sure all input triangles are present in output
#if 0
    for (uint32_t t = 0; t < Nt; t++) {
      auto a = input[3 * t + 0];
      auto b = input[3 * t + 1];
      auto c = input[3 * t + 2];

      bool found = false;
      for (uint32_t tt = 0; tt < Nt; tt++) {
        bool t0 = output[3 * tt + 0] == a;
        bool t1 = output[3 * tt + 1] == b;
        bool t2 = output[3 * tt + 2] == c;
        if (t0 && t1 && t2) {
          found = true;
          break;
        }
      }
      assert(found);
    }
#endif
  }

};



namespace {




}


void linearSpeedVertexCacheOptimisation(Logger logger, uint32_t * output, const uint32_t* input, const uint32_t N)
{
  if (N == 0) return;
  assert((N % 3) == 0);

  LinSpd linspd(logger, output, input, N / 3);
  linspd.run();


}