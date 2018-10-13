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
