#include "Allocators.h"
#include <cassert>
#include <cstring>

namespace {

  size_t max(size_t a, size_t b)
  {
    return a > b ? a : b;
  }

}

void* Arena::alloc(size_t bytes)
{
  const size_t pageSize = 1024 * 1024;

  if (bytes == 0) return nullptr;

  auto padded = (bytes + 7) & ~7;

  if (size < fill + padded) {
    fill = sizeof(uint8_t*);
    size = max(pageSize, fill + padded);

    auto * page = (uint8_t*)xmalloc(size);
    *(uint8_t**)page = nullptr;

    if (first == nullptr) {
      first = page;
      curr = page;
    }
    else {
      *(uint8_t**)curr = page; // update next
      curr = page;
    }
  }

  assert(first != nullptr);
  assert(curr != nullptr);
  assert(*(uint8_t**)curr == nullptr);
  assert(fill + padded <= size);

  auto * rv = curr + fill;
  fill += padded;
  return rv;
}

void* Arena::dup(const void* src, size_t bytes)
{
  auto * dst = alloc(bytes);
  std::memcpy(dst, src, bytes);
  return dst;
}


void Arena::clear()
{
  auto * c = first;
  while (c != nullptr) {
    auto * n = *(uint8_t**)c;
    xfree(c);
    c = n;
  }
  first = nullptr;
  curr = nullptr;
  fill = 0;
  size = 0;
}
