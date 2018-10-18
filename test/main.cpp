#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <cassert>
#include <vector>
#include <list>
#include <algorithm>

#include "Common.h"
#include "Mesh.h"
#include "LinAlgOps.h"
#include "adt/KeyedHeap.h"
#include "topo/HalfEdgeMesh.h"
#include "spatial/R3PointKdTree.h"

namespace {

  struct App
  {
    Tasks tasks;
    std::mutex incomingMeshLock;
    Mesh* mesh = nullptr;
    volatile bool done = false;
  };
  App* app = nullptr;

  void logger(unsigned level, const char* msg, ...)
  {
    switch (level) {
    case 0: fprintf(stderr, "[I] "); break;
    case 1: fprintf(stderr, "[W] "); break;
    case 2: fprintf(stderr, "[E] "); break;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfprintf(stderr, msg, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
  }

  Vec3f cubeVtx[8] = {
    Vec3f(0, 0, 0),
    Vec3f(1, 0, 0),
    Vec3f(0, 1, 0),
    Vec3f(1, 1, 0),
    Vec3f(0, 0, 1),
    Vec3f(1, 0, 1),
    Vec3f(0, 1, 1),
    Vec3f(1, 1, 1)
  };

  uint32_t cubeIdx[6 * 4] = {
    0, 2, 3, 1,
    4, 6, 2, 0,
    5, 7, 6, 4,
    1, 3, 7, 5,
    2, 6, 7, 3,
    5, 4, 0, 1
  };

  uint32_t cubeOff[7] = {
    0, 4, 8, 12, 16, 20, 24
  };

  void runObjReader(Logger logger, std::string path)
  {
    auto time0 = std::chrono::high_resolution_clock::now();
    Mesh* mesh = nullptr;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      logger(2, "Failed to open file %s: %d", path.c_str(), GetLastError());
    }
    else {
      DWORD hiSize;
      DWORD loSize = GetFileSize(h, &hiSize);
      size_t fileSize = (size_t(hiSize) << 32u) + loSize;

      HANDLE m = CreateFileMappingA(h, 0, PAGE_READONLY, 0, 0, NULL);
      if (m == INVALID_HANDLE_VALUE) {
        logger(2, "Failed to map file %s: %d", path.c_str(), GetLastError());
      }
      else {
        const void * ptr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
        if (ptr == nullptr) {
          logger(2, "Failed to map view of file %s: %d", path.c_str(), GetLastError());
        }
        else {
          mesh = readObj(logger, ptr, fileSize);
          UnmapViewOfFile(ptr);
        }
        CloseHandle(m);
      }
      CloseHandle(h);
    }
    if (mesh) {

      auto * name = path.c_str();
      for (auto * t = name; *t != '\0'; t++) {
        if (*t == '\\' || *t == '//') name = t + 1;
      }
      mesh->name = mesh->strings.intern(name);

      auto time1 = std::chrono::high_resolution_clock::now();
      auto e = std::chrono::duration_cast<std::chrono::milliseconds>((time1 - time0)).count();
      std::lock_guard<std::mutex> guard(app->incomingMeshLock);
      app->mesh = mesh;
      logger(0, "Read %s in %lldms", path.c_str(), e);
    }
    else {
      logger(0, "Failed to read %s", path.c_str());
    }
    app->done = true;
  }

  struct HeapItem
  {
    uint32_t key;
    float value;
  };

  // mean zero, variance one, exactly zero outside +/- 6*variance
  float normalDistRand()
  {
    auto sum = 0.f;
    for (unsigned i = 0; i < 12; i++) {
      sum += (1.f / RAND_MAX)*rand();
    }
    return sum - 6.f;
  }

}


int main(int argc, char** argv)
{
  app = new App();
  app->tasks.init(logger);

  TaskFunc taskFunc = []() {runObjReader(logger, "..\\models\\suzanne.obj"); };
  app->tasks.enqueue(taskFunc);

  while (!app->done);
  
  auto * mesh = app->mesh;
  delete app;

  logger(0, "vtxCount=%d", mesh->vtxCount);
  logger(0, "triCount=%d", mesh->triCount);



  {
    logger(0, "Pool checks...");
    Pool<Vec3f> pool;
    auto poolBase = (PoolBase*)&pool;

    Vector<Vec3f*> tmp(7000);
    assert(poolBase->pageCount == 0);
    for (size_t i = 0; i < tmp.size(); i++) {
      tmp[i] = pool.alloc();
    }
    for (size_t i = 0; i < tmp.size(); i++) {
      auto ix = pool.getIndex(tmp[i]);
      assert(ix == i);
      auto ptr = pool.fromIndex(ix);
      assert(ptr == tmp[i]);
      ptr = &pool[ix];
      assert(ptr == tmp[i]);
    }

    auto pagesN = poolBase->pageCount;
    assert(poolBase->pageCount != 0);
    assert(poolBase->itemsAlloc == tmp.size32());
    for (size_t i = 0; i < tmp.size(); i++) {
      pool.release(tmp[i]);
    }
    assert(poolBase->itemsAlloc == 0);
    for (size_t i = 0; i < tmp.size(); i++) {
      tmp[i] = pool.alloc();
    }
    for (size_t i = 0; i < tmp.size(); i++) {
      auto ix = pool.getIndex(tmp[i]);
      auto ptr = pool.fromIndex(ix);
      assert(ptr == tmp[i]);
    }
    assert(poolBase->pageCount == pagesN);
    assert(poolBase->itemsAlloc == tmp.size32());
    for (size_t i = tmp.size(); 0 < i; i--) {
      pool.release(tmp[i-1]);
    }
    assert(poolBase->itemsAlloc == 0);
    logger(0, "Pool checks... OK");
  }

  if(true) {
    logger(0, "Keyed heap checks...");

    srand(42);
    uint32_t N = 100;

    KeyedHeap heap;
    heap.setKeyRange(N);
 
    std::list<HeapItem> items;
    for (uint32_t i = 0; i < N; i++) {
      HeapItem item{ i, float(rand()) };
      items.push_back(item);
      heap.insert(item.key, item.value);
      heap.assertHeapInvariants();
    }

    items.sort([](const HeapItem & a, const HeapItem &  b) { return a.value < b.value; });

    // regular delmin on the first half
    auto Na = N / 2;
    for (uint32_t i = 0; i < Na; i++) {
      auto item = items.front();
      items.pop_front();

      auto key = heap.removeMin();
      auto b = heap.getValue(key);

      assert(item.key == key);
      assert(item.value == b);
      heap.assertHeapInvariants();
    }

    assert(!items.empty());
    unsigned m = 0;
    for (auto it = items.begin(); it != items.end();) {
      // change value
      if (m == 1) {
        it->value = float(rand());
        heap.update(it->key, it->value);
        it++;
      }
      // erase
      else if (m == 2) {
        heap.erase(it->key);
        it = items.erase(it);
      }
      // keep
      else {
        it++;
      }
      m = (m + 1) % 3;
    }
    items.sort([](const HeapItem & a, const HeapItem &  b) { return a.value < b.value; });

    for (auto & item : items) {
      auto key = heap.removeMin();
      auto b = heap.getValue(key);

      assert(item.key == key);
      assert(item.value == b);
      heap.assertHeapInvariants();
    }

    logger(0, "Keyed heap checks... OK");
  }

  {
    logger(0, "Half-edge cube checks...");

    HalfEdge::R3Mesh hemesh(logger);
    hemesh.insert(cubeVtx, 8,  cubeIdx, cubeOff, 6);

    assert(hemesh.getBoundaryEdgeCount() == 0);
    assert(hemesh.getManifoldEdgeCount() == 12);
    assert(hemesh.getNonManifoldEdgeCount() == 0);


    logger(0, "Half-edge cube checks... OK");
  }

  {
    logger(0, "KD-tree checks...");

    Vector<Vec3f> P;
    P.reserve(10 * mesh->vtxCount);

    auto sigma = 1.f;
#if 1
    for (uint32_t i = 0; i < mesh->vtxCount; i++) {
      for (uint32_t k = 0; k < 1; k++) {
        P.pushBack(mesh->vtx[i] + sigma * Vec3f(normalDistRand(),
                                                normalDistRand(),
                                                normalDistRand()));
      }
    }
#else

    for (uint32_t i = 0; i < 8; i++) {
      for (uint32_t k = 0; k < 1; k++) {
        P.pushBack(cubeVtx[i] + sigma * Vec3f(normalDistRand(),
                                                normalDistRand(),
                                                normalDistRand()));
      }

    }
#endif

    for (unsigned l = 0; l < 2; l++) {

      logger(0, "KdTree built %s", l == 0 ? "spatially balanced" : "for minimal depth");

      KdTree::R3StaticTree kdTree(logger, P.data(), P.size32(), l==0);
      kdTree.assertInvariants();

      logger(0, "Find k-nearest neighbours test.");
      float avgRadius = 0.f;  // to be used in next test.
      auto K = 7u;
      assert(K < P.size32());
      Vector<KdTree::QueryResult> queryResult;
      std::vector<std::pair<float, uint32_t>> distances(P.size());
      for (unsigned j = 0; j < P.size32(); j++) {

        kdTree.getNearestNeighbours(queryResult, P[j], 7);

        for (unsigned i = 0; i < P.size32(); i++) {
          distances[i] = std::make_pair(distanceSquared(P[i], P[j]), i);
        }
        std::sort(distances.begin(), distances.end(), [](auto&a, auto&b) { return a.first < b.first; });
        for (unsigned i = 0; i < K; i++) {
          if (i != 0) {
            assert(distances[i - 1].first < distances[i].first);
          }
          bool found = false;
          for (auto k : queryResult) {
            if (k.ix == distances[i].second) found = true;
          }
          assert(found);
        }
        avgRadius += std::sqrt(queryResult[K - 1].distanceSquared);
      }
      avgRadius *= 1.f / P.size32();


      logger(0, "Find points within radius test.");
      for (unsigned j = 0; j < P.size32(); j++) {

        kdTree.getPointsWithinRadius(queryResult, P[j], avgRadius);

        for (unsigned i = 0; i < P.size32(); i++) {
          distances[i] = std::make_pair(distanceSquared(P[i], P[j]), i);
        }
        std::sort(distances.begin(), distances.end(), [](auto&a, auto&b) { return a.first < b.first; });

        unsigned n;
        auto d2 = avgRadius * avgRadius;
        for (n = 0; n < P.size32() && distances[n].first <= d2; n++) {

          bool found = false;
          for (auto k : queryResult) {
            if (k.ix == distances[n].second) found = true;
          }
          assert(found);
        }
        assert(n == queryResult.size32());
      }
    }

    logger(0, "KD-tree checks OK");
  }



  return 0;
}
