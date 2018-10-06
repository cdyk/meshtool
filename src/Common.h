#pragma once
#include <cstdint>
#include <functional>

typedef void(*Logger)(unsigned level, const char* msg, ...);

void* xmalloc(size_t size);

void* xcalloc(size_t count, size_t size);

void* xrealloc(void* ptr, size_t size);

struct Mesh;

struct Arena
{
  Arena() = default;
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;

  ~Arena() { clear(); }

  uint8_t * first = nullptr;
  uint8_t * curr = nullptr;
  size_t fill = 0;
  size_t size = 0;

  void* alloc(size_t bytes);
  void* dup(const void* src, size_t bytes);
  void clear();

  template<typename T> T * alloc() { return new(alloc(sizeof(T))) T(); }
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
  size_t getCount()
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
class Vector : public BufferBase
{
public:
  Vector() = default;
  Vector(const Vector&) = delete;   // haven't bothered yet
  Vector& operator=(const Vector&) = delete;  //  haven't bothered yet
  ~Vector() { resize(0); }

  Vector(size_t size) { resize(size); }

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

  size_t size() const { return fill; }

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

void* xmalloc(size_t size);

void* xcalloc(size_t count, size_t size);

void* xrealloc(void* ptr, size_t size);

uint64_t fnv_1a(const char* bytes, size_t l);

Mesh* readObj(Logger logger, const void * ptr, size_t size);