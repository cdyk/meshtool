#pragma once

#include <cstdint>
#include "../Common.h"

// Min-heap implementation where one can efficiently
// change values of elements already present in the heap.
class KeyedHeap
{
public:
  typedef uint32_t Key;

  KeyedHeap() {}

  // Clear heap and set key domain to [0...N-1]
  void setKeyRange(Key N);
  
  // Erase an existing element from the heap.
  void erase(Key ix);

  // Insert a new element in the heap.
  void insert(Key key, float value);

  // Update an existing element in the heap.
  void update(Key key, float value);

  // Remove smallest element and return its key.
  Key removeMin();

  inline float getValue(uint32_t key) const { return lut[key].value; }

  inline Key peekMin() const { assert(fill); return heap[0]; }

  inline bool any() const { return fill; }

  inline bool empty() const { return fill == 0; }

  inline uint32_t size() const { return fill; }

  void assertHeapInvariants();

protected:
  typedef uint32_t HeapPos;
  struct LutEntry
  {
    HeapPos heapPos = ~0u;
    float value = 0.f;
  };

 
  Key keyRange = 0;
  HeapPos fill = 0;
  MemBuffer<LutEntry> lut;
  MemBuffer<Key> heap;

  void percolateUp(HeapPos heapPos);

  void percolateDown(HeapPos heapPos);
};


