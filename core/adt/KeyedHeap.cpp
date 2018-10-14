#include <limits>
#include <cassert>

#include "keyedheap.h"


void KeyedHeap::setKeyDomain(uint32_t size)
{
  lut.clear();
  heap.clear();
  lut.resize(size);
}

void KeyedHeap::heapSwap(uint32_t heapPosA, uint32_t heapPosB)
{
  std::swap(heap[heapPosA], heap[heapPosB]);
  lut[heap[heapPosA]].heapPos = heapPosA;
  lut[heap[heapPosB]].heapPos = heapPosB;
}

float KeyedHeap::valueAtHeapPos(uint32_t heapPos)
{
  return lut[heap[heapPos]].value;
}

void KeyedHeap::percolateUp(uint32_t child)
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

void KeyedHeap::percolateDown(uint32_t parent)
{
  auto N = uint32_t(heap.size());
  while (true) {
    auto childA = 2 * parent + 1;

    if (N <= childA) return;

    auto smallestChild = childA;
    auto smallestChildKey = heap[childA];

    auto childB = 2 * parent + 2;
    if (childB < N) {
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

void KeyedHeap::insert(uint32_t key, float value)
{
  lut[key].value = value;
  lut[key].heapPos = uint32_t(heap.size());
  heap.push_back(key);
  percolateUp(lut[key].heapPos);
}

void KeyedHeap::decreaseValue(uint32_t key, float value)
{
  lut[key].value = value;
  percolateUp(lut[key].heapPos);
}

void KeyedHeap::increaseValue(uint32_t key, float value)
{
  lut[key].value = value;
  percolateDown(lut[key].heapPos);
}

void KeyedHeap::erase(uint32_t key)
{
  assert(lut[key].heapPos != illegal_index && "Element not present in heap");

  auto t = lut[key].value;

  decreaseValue(key, -std::numeric_limits<float>::max());
  assert(removeMin() == key);

  lut[key].value = t; // hmm, I don't remember why...
}

void KeyedHeap::assertHeapInvariants()
{
  for (uint32_t child = 1; child < uint32_t(heap.size()); child++) {
    auto parent = (child - 1) / 2;
    auto childKey = heap[child];
    auto parentKey = heap[parent];
    assert(lut[parentKey].value < lut[childKey].value);
  }
}

void KeyedHeap::update(uint32_t key, float value)
{
  assert(lut[key].heapPos != illegal_index && "Element not present in heap");
  if (value < lut[key].value) {
    decreaseValue(key, value);
  }
  else if(lut[key].value < value) {
    increaseValue(key, value);
  }
}

uint32_t KeyedHeap::peekMin() const
{
  assert(!heap.empty());
  return heap[0];
}

uint32_t KeyedHeap::removeMin()
{
  assert(!heap.empty());

  Key headKey = heap[0];
  lut[headKey].heapPos = ~0u;

  Key tailKey = heap[uint32_t(heap.size() - 1)];
  heap.pop_back();

  if (!heap.empty()) {
    heap[0] = tailKey;
    lut[tailKey].heapPos = 0;
    percolateDown(0);
  }

  return headKey;
}
