#pragma once
#include <cstdint>

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

  PoolBase(uint32_t elementSize, uint32_t itemsPerPage) :
    elementSize(elementSize), itemsPerPage(itemsPerPage)
  {}
  ~PoolBase();

  struct PageHeader {
    union {
      struct {
        PageHeader* next;
      };
      char alignment[16];
    };
  };

  struct FreeItem {
    FreeItem* next;
  };

  void* alloc_();
  void release_(void*);


  PageHeader* pages = nullptr;
  FreeItem* free = nullptr;
  uint32_t elementSize = 0;
  uint32_t itemsPerPage = 0;
  uint32_t pagesAlloc = 0;
  uint32_t itemsAlloc = 0;
};

template<typename T>
struct Pool : PoolBase
{
  static_assert(sizeof(void*) <= sizeof(T));


  Pool(uint32_t itemsPerPage = 1024) :
    PoolBase(uint32_t(sizeof(T)), itemsPerPage)
  {}

  T* alloc() { return (T*)alloc_(); }
  void release(void* p) { release_(p); }
};