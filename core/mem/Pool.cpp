#include "Allocators.h"
#include <cassert>
#include <cstddef>

PoolBase::~PoolBase()
{
  auto * page = pages;
  while (page != nullptr) {
    auto * next = page->next;
    xfree(page);
    page = next;
  }
  pages = nullptr;
}


void* PoolBase::alloc_()
{
  if (free == nullptr) {
    assert(sizeof(PageHeader) == 16);
    assert(sizeof(FreeItem) <= elementSize);
    assert(0 < itemsPerPage);

    auto * next = pages;
    pages = (PageHeader*)xmalloc(sizeof(PageHeader) + elementSize * itemsPerPage);
    pages->next = next;
    pagesAlloc++;

    auto * ptr = (char*)pages + sizeof(PageHeader);
    for (size_t i = 0; i < itemsPerPage; i++) {
      auto * next = free;
      free = (FreeItem*)ptr;
      free->next = next;
      ptr += elementSize;
    }
  }
  itemsAlloc++;

  assert(free);
  auto * rv = free;
  free = rv->next;
  return rv;
}

void PoolBase::release_(void* ptr)
{
  assert(ptr);
  assert(itemsAlloc);
  --itemsAlloc;

  auto * item = (FreeItem*)ptr;
  item->next = free;
  free = item;
}
