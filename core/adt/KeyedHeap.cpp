#include <limits>
#include <cassert>

#include "keyedheap.h"


uint32_t KeyedHeap::getParent(uint32_t heapPos)
{
  return heapPos > 0 ? (heapPos - 1) / 2 : illegal_index;
}

uint32_t KeyedHeap::getLeftChild(uint32_t heapPos)
{
  return 2 * heapPos + 1 < uint32_t(heap.size()) ? 2 * heapPos + 1 : illegal_index;
}

uint32_t KeyedHeap::getRightChild(uint32_t heapPos)
{
  return 2 * heapPos + 2 < uint32_t(heap.size()) ? 2 * heapPos + 2 : illegal_index;
}


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

float KeyedHeap::getValue(uint32_t key)
{
  return lut[key].value;
}

float KeyedHeap::valueAtHeapPos(uint32_t heapPos)
{
  return lut[heap[heapPos]].value;
}

void KeyedHeap::percolateUp(uint32_t heapPos)
{
  // Start at heapPos and enforce heap invariant up to the root.
  do {
    auto parent = getParent(heapPos);
    if (parent == illegal_index)
      return; // top of heap
    if (valueAtHeapPos(parent) < valueAtHeapPos(heapPos))
      return; // invariant o.k.
    heapSwap(heapPos, parent);
    heapPos = parent;
  } while (1);
}

void KeyedHeap::percolateDown(uint32_t heapPos)
{
  // Start at heapPos and enforce heap invariant down to the first leaf along min value branches.
  do {
    auto left = getLeftChild(heapPos);
    auto right = getRightChild(heapPos);

    if (left == illegal_index) { return; } // Reached bottom of heap.

   // get smallest child
    uint32_t child;
    if (right != illegal_index) {
      child = valueAtHeapPos(left) < valueAtHeapPos(right) ? left : right;
    }
    else {
      child = left;
    }

    if (valueAtHeapPos(heapPos) < valueAtHeapPos(child)) {
      // invariant o.k.
      return;
    }
    heapSwap(heapPos, child);
    heapPos = child;
  } while (1);
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
  for (uint32_t i = 1; i < uint32_t(heap.size()); i++) {
    auto a = valueAtHeapPos(i);
    auto b = valueAtHeapPos(getParent(i));
    assert(a <= b);
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
  auto index = heap[0];
  heapSwap(0, uint32_t(heap.size()) - 1);
  heap.pop_back();
  lut[index].heapPos = illegal_index;
  percolateDown(0);
  return index;
}
