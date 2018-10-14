#include <limits>
#include <cassert>

#include "keyedheap.h"


void KeyedHeap::setKeyRange(Key size)
{
  keyRange = size;
  lut.accommodate(size);
  for (Key i = 0; i < keyRange; i++) {
    lut[i].heapPos = ~0u;
    lut[i].value = 0.f;
  }

  heap.accommodate(size);
  fill = 0;
}

void KeyedHeap::percolateUp(HeapPos child)
{
  while (child) {
    auto childKey = heap[child];
    auto parent = (child - 1) / 2;
    auto parentKey = heap[parent];
    if (lut[parentKey].value < lut[childKey].value) {
      return;
    }

    heap[child] = parentKey;
    heap[parent] = childKey;
    lut[childKey].heapPos = parent;
    lut[parentKey].heapPos = child;

    child = parent;
  }
}

void KeyedHeap::percolateDown(HeapPos parent)
{
  while (true) {
    auto childA = 2 * parent + 1;

    if (fill <= childA) return;

    auto smallestChild = childA;
    auto smallestChildKey = heap[childA];

    auto childB = 2 * parent + 2;
    if (childB < fill) {
      auto childBKey = heap[childB];
      if (lut[childBKey].value < lut[smallestChildKey].value) {
        smallestChild = childB;
        smallestChildKey = childBKey;
      }
    }

    auto parentKey = heap[parent];
    if (lut[parentKey].value < lut[smallestChildKey].value) return;

    heap[smallestChild] = parentKey;
    heap[parent] = smallestChildKey;
    lut[smallestChildKey].heapPos = parent;
    lut[parentKey].heapPos = smallestChild;

    parent = smallestChild;
  }
}

void KeyedHeap::insert(Key key, float value)
{
  assert(lut[key].heapPos == ~0u);
  lut[key].value = value;
  lut[key].heapPos = fill;
  heap[fill++] = key;
  percolateUp(lut[key].heapPos);
}

void KeyedHeap::erase(Key key)
{
  assert(lut[key].heapPos != ~0u && "Element not present in heap");

  auto t = lut[key].value;
  update(key, -FLT_MAX);
  assert(removeMin() == key);

  lut[key].value = t; // in case we call getValue with key
}

void KeyedHeap::assertHeapInvariants()
{
  for (HeapPos child = 1; child < fill; child++) {
    auto parent = (child - 1) / 2;
    auto childKey = heap[child];
    auto parentKey = heap[parent];
    assert(lut[parentKey].value < lut[childKey].value);
  }
}

void KeyedHeap::update(Key key, float value)
{
  assert(lut[key].heapPos != ~0u && "Element not present in heap");
  if (value < lut[key].value) {
    lut[key].value = value;
    percolateUp(lut[key].heapPos);
  }
  else if(lut[key].value < value) {

    lut[key].value = value;
    percolateDown(lut[key].heapPos);
  }
}

uint32_t KeyedHeap::removeMin()
{
  assert(fill);

  Key headKey = heap[0];
  lut[headKey].heapPos = ~0u;

  Key tailKey = heap[--fill];

  if (fill) {
    heap[0] = tailKey;
    lut[tailKey].heapPos = 0;
    percolateDown(0);
  }

  return headKey;
}
