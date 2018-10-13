#pragma once
#include <cstdint>
#include <cassert>

void* xmalloc(size_t size);

void* xcalloc(size_t count, size_t size);

void* xrealloc(void* ptr, size_t size);

void xfree(void*);

struct Arena
{
  Arena() = default;
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  ~Arena() { clear(); }

  uint8_t * first = nullptr;
  uint8_t * curr = nullptr;
  size_t fill = 0;
  size_t size = 0;

  void* alloc(size_t bytes);
  void* dup(const void* src, size_t bytes);
  void clear();

  template<typename T> T * alloc() { return new(alloc(sizeof(T))) T(); }
};

struct PoolBase
{
  PoolBase() = delete;
  PoolBase(const PoolBase&) = delete;
  PoolBase& operator=(const PoolBase&) = delete;

  PoolBase(uint32_t elementSize, uint32_t itemsPerPage);
  ~PoolBase();

  struct FreeItem {
    FreeItem* next;
  };

  void allocPage_();

  inline void* alloc_()
  {
    if (free == nullptr) allocPage_();
    itemsAlloc++;
    auto * rv = free;
    free = rv->next;
    return rv;
  }

  inline void release_(void* ptr)
  {
    assert(ptr);
    assert(itemsAlloc);
    --itemsAlloc;

    auto * item = (FreeItem*)ptr;
    item->next = free;
    free = item;
  }

  inline void* fromIndex_(uint32_t ix)
  {
    auto page = ix >> itemsPerPageLog2;
    auto subIx = ix & ((1u << itemsPerPageLog2) - 1u);
    assert(page < pageCount);
    auto * payloadStart = (char*)(pages[page]);
    return payloadStart + elementSize * subIx;
  }

  inline uint32_t getIndex_(void* ptr)
  {
    size_t pageSize = elementSize << itemsPerPageLog2;
    for (unsigned p = 0; p < pageCount; p++) {
      if (pages[p] <= ptr && ptr < pages[p] + pageSize) {

        auto subOff = uint32_t(((char*)ptr - (char*)pages[p]));
        auto subIx = subOff / elementSize;
        assert((subOff % elementSize) == 0 && "Illegal pointer");

        return (p << itemsPerPageLog2) + subIx;
      }
    }

    assert(false && "Illegal pointer");
    return 0;
  }

  char** pages = nullptr;
  FreeItem* free = nullptr;
  uint32_t pageCount = 0;
  uint32_t elementSize = 0;
  uint32_t itemsPerPageLog2 = 0;
  uint32_t itemsAlloc = 0;
};

template<typename T>
struct Pool : PoolBase
{
  static_assert(sizeof(void*) <= sizeof(T));


  Pool(uint32_t itemsPerPage = 1024) :
    PoolBase(uint32_t(sizeof(T)), itemsPerPage)
  {}

  T& operator[](uint32_t ix) { return *(T*)fromIndex_(ix); }
  const T& operator[](uint32_t ix) const { return *(T*)fromIndex_(ix); }

  T* alloc() { return (T*)alloc_(); }
  void release(void* p) { release_(p); }
  inline T* fromIndex(uint32_t ix) { return (T*)fromIndex_(ix); }
  inline uint32_t getIndex(T* ptr) { return getIndex_(ptr); }
};