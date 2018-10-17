#pragma once
#include <cstdint>
#include <functional>
#include "mem/Allocators.h"

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



typedef std::function<void(void)> TaskFunc;
typedef unsigned TaskId;
struct TasksImpl;

struct Tasks
{
  ~Tasks();

  void init(Logger logger);
  TaskId enqueue(TaskFunc& func);
  void wait(TaskId id);
  void update();
  void cleanup();

  TasksImpl* impl = nullptr;
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


struct BufferBase
{
public:
  size_t allocated()
  {
    if (ptr) return ((size_t*)ptr)[-1];
    else return 0;
  }

protected:
  char* ptr = nullptr;

  ~BufferBase() { free(); }

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
struct Buffer : public BufferBase
{
  Buffer() = default;
  Buffer(size_t size) { accommodate(size); }

  T* data() { return (T*)ptr; }
  T& operator[](size_t ix) { return data()[ix]; }
  const T* data() const { return (T*)ptr; }
  const T& operator[](size_t ix) const { return data()[ix]; }
  void accommodate(size_t count, bool keep = false) { _accommodate(sizeof(T), count, keep); }
};

// calls constructors/destructors
template<typename T>
class Vector : BufferBase
{
public:
  Vector() = default;
  Vector(const Vector&) = delete;   // haven't bothered yet
  Vector& operator=(const Vector&) = delete;  //  haven't bothered yet
  ~Vector() { resize(0); }

  Vector(size_t size) { resize(size); }

  T* begin() { return data(); }
  T* end() { return data() + fill; }
  const T* begin() const { return data(); }
  const T* end() const { return data() + fill; }

  T* data() { return (T*)ptr; }
  T& operator[](size_t ix) { return data()[ix]; }
  const T* data() const { return (T*)ptr; }
  const T& operator[](size_t ix) const { return data()[ix]; }

  void resize(size_t newSize)
  {
    _accommodate(sizeof(T), newSize, true);
    for (size_t i = newSize; i < fill; i++) (*this)[i].~T();
    for (size_t i = fill; i < newSize; i++) new(&(*this)[i]) T();
    fill = newSize;
  }

  void reserve(size_t size)
  {
    if (size < allocated()) {
      _accommodate(sizeof(T), 2 * fill < 16 ? 16 : 2 * fill, true);
    }
  }

  void pushBack(T t)
  {
    if (fill <= allocated()) {
      _accommodate(sizeof(T), 2 * fill < 16 ? 16 : 2 * fill, true);
    }
    new(data() + (fill++)) T(t);
  }

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