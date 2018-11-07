#pragma once
#include <cstdint>
#include <functional>
#include <initializer_list>
#include "mem/Allocators.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

typedef void(*Logger)(unsigned level, const char* msg, ...);



struct Mesh;
struct MeshItem;

class NonCopyable
{
public:
  NonCopyable() = default;
  ~NonCopyable() = default;
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};



template<typename T>
struct ListHeader
{
  T* first;
  T* last;

  ListHeader() : first(nullptr), last(nullptr) {}

  bool empty() { return first == nullptr; }

  void append(T* item)
  {
    if (first == nullptr) {
      first = last = item;
    }
    else {
      last->next = item;
      last = item;
    }
  }
};


struct MemBufferBase
{
public:
  size_t allocated()
  {
    if (ptr) return ((size_t*)ptr)[-1];
    else return 0;
  }

protected:
  char* ptr = nullptr;

  MemBufferBase() {}
  ~MemBufferBase() { free(); }

  void _swap(MemBufferBase& other) { auto t = ptr; ptr = other.ptr; other.ptr = t; }
  void free();

  void _accommodate(size_t typeSize, size_t count, bool keep)
  {
    if (count == 0) return;
    if (ptr && count <= ((size_t*)ptr)[-1]) return;

    if (keep && ptr) {
      ptr = (char*)xrealloc(ptr - sizeof(size_t), typeSize * count + sizeof(size_t)) + sizeof(size_t);
    }
    else {
      free();
      ptr = (char*)xmalloc(typeSize * count + sizeof(size_t)) + sizeof(size_t);
    }
    ((size_t*)ptr)[-1] = count;
  }

};

template<typename T>
struct MemBuffer : public MemBufferBase
{
  MemBuffer() = default;
  MemBuffer(size_t size) { accommodate(size); }

  T* data() { return (T*)ptr; }
  T& operator[](size_t ix) { return data()[ix]; }
  const T* data() const { return (T*)ptr; }
  const T& operator[](size_t ix) const { return data()[ix]; }
  void accommodate(size_t count, bool keep = false) { _accommodate(sizeof(T), count, keep); }
};

// calls constructors/destructors
template<typename T>
class Vector : MemBufferBase
{
public:
  Vector() = default;
  Vector(size_t size) { resize(size); }
  ~Vector() { resize(0); }

  Vector(size_t size, const T& init) {
    assert(fill == 0);
    fill = size;
    _accommodate(sizeof(T), fill, false);
    for (size_t i = 0; i < fill; i++) new(&(*this)[i]) T(init);
  }


  Vector(const Vector& other) {
    assert(fill == 0);
    fill = other.size();
    _accommodate(sizeof(T), fill, true);
    for(size_t i=0; i<fill; i++) new(&(*this)[i]) T(other[i]);
  }

  Vector(std::initializer_list<T> init)
  {
    assert(fill == 0);
    fill = init.size();
    _accommodate(sizeof(T), fill, false);
    T * dst = data();
    for (const T& val : init) new(dst++) T(val);
  }

  Vector& operator=(const Vector& other) {
    auto newSize = other.size();
    if (newSize <= fill) {
      for (size_t i = 0; i < newSize; i++) {
        (*this)[i].~T();
        new(&(*this)[i]) T(other[i]);
      }
      for (size_t i = newSize; i < fill; i++) (*this)[i].~T();
    }
    else {
      for (size_t i = 0; i < fill; i++) (*this)[i].~T();
      free();
      _accommodate(sizeof(T), newSize, true);
      for (size_t i = 0; i < newSize; i++) {
        new(&(*this)[i]) T(other[i]);
      }
    }
    fill = newSize;
    assert(fill <= allocated());
  }
 
  Vector(Vector&& other) noexcept { swap(other); }
  Vector& operator=(Vector&& other) noexcept { swap(other); return *this; }
  void swap(Vector& other) noexcept { _swap(other); auto t = fill; fill = other.fill; other.fill = t; }

  T* begin() { return data(); }
  T* end() { return data() + fill; }
  const T* begin() const { return data(); }
  const T* end() const { return data() + fill; }

  T* data() { return (T*)ptr; }
  T& operator[](size_t ix) { return data()[ix]; }
  const T* data() const { return (T*)ptr; }
  const T& operator[](size_t ix) const { return data()[ix]; }

  void clear() { resize(0); }

  void resize(size_t newSize)
  {
    _accommodate(sizeof(T), newSize, true);
    for (size_t i = newSize; i < fill; i++) (*this)[i].~T();
    for (size_t i = fill; i < newSize; i++) new(&(*this)[i]) T();
    fill = newSize;
  }

  void reserve(size_t size)
  {
    if (allocated() < size) {
      _accommodate(sizeof(T), 2 * fill < 16 ? 16 : 2 * fill, true);
    }
  }

  void pushBack(const T & t)
  {
    reserve(fill + 1);
    new(data() + (fill++)) T(t);
  }

  void pushBack(T && t)
  {
    reserve(fill + 1);
    new(data() + (fill++)) T(t);
  }

  T& back() { assert(fill); return (*this)[fill - 1]; }
  const T& back() const { assert(fill); return (*this)[fill - 1]; }
  T popBack()
  {
    assert(fill);
    auto t = data()[--fill];
    (*this)[fill].~T();
    return t;
  }

  bool empty() const { return fill == 0; }
  bool any() const { return fill != 0; }

  size_t size() const { return fill; }
  uint32_t size32() const { return uint32_t(fill); }

private:
  size_t fill = 0;

};



struct Map
{
  Map() = default;
  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;


  ~Map();

  uint64_t* keys = nullptr;
  uint64_t* vals = nullptr;
  size_t fill = 0;
  size_t capacity = 0;

  void clear();

  bool get(uint64_t& val, uint64_t key);
  uint64_t get(uint64_t key);

  void insert(uint64_t key, uint64_t value);
};

struct StringInterning
{
  Arena arena;
  Map map;

  const char* intern(const char* a, const char* b);
  const char* intern(const char* str);  // null terminanted
};


uint64_t fnv_1a(const char* bytes, size_t l);

Mesh* readObj(Logger logger, const void * ptr, size_t size);