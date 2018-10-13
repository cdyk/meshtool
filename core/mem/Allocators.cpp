#include "Allocators.h"
#include <cstdlib>
#include <cassert>

void* xmalloc(size_t size)
{
  auto rv = malloc(size);
  assert(rv != nullptr &&  "Failed to allocate memory.");
  return rv;
}

void* xcalloc(size_t count, size_t size)
{
  auto rv = calloc(count, size);
  assert(rv != nullptr &&  "Failed to allocate memory.");
  return rv;
}

void* xrealloc(void* ptr, size_t size)
{
  auto * rv = realloc(ptr, size);
  assert(rv != nullptr &&  "Failed to allocate memory.");
  return rv;
}

void xfree(void* ptr)
{
  free(ptr);
}
