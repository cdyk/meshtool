#pragma once

#include <vector>
#include <cstdint>

// Min-heap implementation where one can efficiently
// change values of elements already present in the heap.
class KeyedHeap
{
public:

  // Create heap with empty key domain
  KeyedHeap() {}

  // Clear heap and set key domain to [0...N-1]
  void setKeyDomain(uint32_t N);

  // Get value associated with key.
  float getValue(uint32_t key);

  // Erase an existing element from the heap.
  void erase(uint32_t ix);

  // Insert a new element in the heap.
  void insert(uint32_t key, float value);

  // Update an existing element in the heap.
  void update(uint32_t key, float value);

  // Remove smallest element and return its key.
  uint32_t removeMin();

  // Get key of smallest element,
  uint32_t peekMin() const;

  bool empty() const { return heap.empty(); }

  void assertHeapInvariants();

protected:
  static const uint32_t illegal_index = ~0u;

  struct LutEntry
  {
    uint32_t heapPos = illegal_index;
    float value = 0.f;
  };

  std::vector<LutEntry> lut;
  std::vector<uint32_t> heap;

  // Swap values in the heap and update lut.
  void heapSwap(uint32_t heapPosA, uint32_t heapPosB);

  float valueAtHeapPos(uint32_t heapPos);

  void percolateUp(uint32_t heapPos);

  void percolateDown(uint32_t heapPos);

  void decreaseValue(uint32_t key, float value);

  void increaseValue(uint32_t key, float value);

  uint32_t getParent(uint32_t heapPos);

  uint32_t getLeftChild(uint32_t heapPos);

  uint32_t getRightChild(uint32_t heapPos);

};


