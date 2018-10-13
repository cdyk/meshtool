#include "Allocators.h"
#include <cassert>
#include <cstddef>

PoolBase::PoolBase(uint32_t elementSize, uint32_t itemsPerPage) :
  elementSize(elementSize)
{
  assert(0 < itemsPerPage);
  auto t = itemsPerPage;
  for (itemsPerPageLog2 = 0; (t & 1) == 0; t = t >> 1, itemsPerPageLog2++) {}
  assert(((t & ~1) == 0) && "itemsPerPage is not a power of two");
  assert((1 << itemsPerPageLog2) == itemsPerPage);
}

PoolBase::~PoolBase()
{
  xfree(pages);
  pages = nullptr;
}


void PoolBase::allocPage_()
{
  auto itemsPerPage = 1 << itemsPerPageLog2;

  assert(sizeof(FreeItem) <= elementSize);

  auto * page = (char*)xmalloc(elementSize * itemsPerPage);
  pages = (char**)xrealloc(pages, sizeof(char*)*(pageCount + 1));
  pages[pageCount++] = page;

  for (size_t i = itemsPerPage - 1; i < itemsPerPage; i--) {
    auto * next = free;
    free = (FreeItem*)((char*)page + elementSize * i);
    free->next = next;
  }
  assert(free);
}
