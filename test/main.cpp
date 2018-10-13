#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <cassert>
#include <vector>
#include <algorithm>

#include "Common.h"
#include "Mesh.h"
#include "adt/KeyedHeap.h"


namespace {

  struct App
  {
    Tasks tasks;
    std::mutex incomingMeshLock;
    Mesh* mesh = nullptr;
    bool done = false;
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

}


int main(int argc, char** argv)
{
  app = new App();
  app->tasks.init(logger);

  TaskFunc taskFunc = []() {runObjReader(logger, "..\\models\\suzanne.obj"); };
  app->tasks.enqueue(taskFunc);

  while (!app->done);

  logger(0, "vtxCount=%d", app->mesh->vtxCount);
  logger(0, "triCount=%d", app->mesh->triCount);

  delete app;

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
    uint32_t N = 700;
    std::vector<float> values(N);

    KeyedHeap heap;
    heap.setKeyDomain(N);
 
    for (uint32_t i = 0; i < N; i++) {
      values[i] = float(rand());
      heap.insert(i, values[i]);
      heap.assertHeapInvariants();
    }
    std::sort(values.begin(), values.end());
    for (uint32_t i = 0; i < N; i++) {
      auto a = values[i];
      auto b = heap.removeMin();

      assert(a == b);
      heap.assertHeapInvariants();
    }
    logger(0, "Keyed heap checks... OK");
  }

  return 0;
}
