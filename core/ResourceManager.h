#pragma once
#include <cassert>
#include <cstdint>
#include <atomic>
#include <mutex>
#include "Common.h"

class ResourceManagerBase;

class ResourceBase : NonCopyable
{
public:
  ResourceBase(ResourceManagerBase& manager);

  enum struct Flags : uint32_t {
    None = 0,
    External = 1
  };

  bool hasFlag(Flags flag) const { return (uint32_t)flags & (uint32_t)flag; }
  void setFlag(Flags flag) { flags = (Flags)((uint32_t)flags | (uint32_t)flag); }

  void increment();
  void decrement();

private:
  ResourceManagerBase& manager;
  std::atomic<uint32_t> refs = 0;
  Flags flags = Flags::None;
};

template<typename T>
struct ResourceHandle
{
  T* resource = nullptr;

  ResourceHandle() = default;
  ~ResourceHandle() { release(); }

  ResourceHandle(const ResourceHandle& rhs) { resource = rhs.resource; if(resource) resource->increment(); }
  ResourceHandle& operator=(const ResourceHandle& rhs) { release(); resource = rhs.resource; if(resource) resource->increment(); return *this; }

  ResourceHandle(ResourceHandle&& rhs) { resource = rhs.resource; rhs.resource = nullptr; }
  ResourceHandle& operator=(ResourceHandle&& rhs) { release(); resource = rhs.resource; rhs.resource = nullptr; return *this; }

  explicit operator bool() const { return resource; }
  bool operator!=(const ResourceHandle& rhs) const { return resource != rhs.resource; }
  bool operator==(const ResourceHandle& rhs) const { return resource == rhs.resource; }

  ResourceHandle(T* resource) : resource(resource) { resource->increment(); }

  void release()
  {
    if (resource) {
      resource->decrement();
      resource = nullptr;
    }
  }

};

class ResourceManagerBase
{
  friend class ResourceBase;
public:
  ~ResourceManagerBase();

  void houseKeep();

  uint32_t getCount();
  uint32_t getOrphanCount();

protected:
  typedef void(*ReleaseFuncBasePtr)(void* data, ResourceBase* resource);

  ResourceManagerBase(ReleaseFuncBasePtr func, void* data) : func(func), data(data) {}

  void track(ResourceBase* resource);
  void orphan(ResourceBase* resource);
  void getOrphansBase(Vector<ResourceBase*>* o);

private:
  ReleaseFuncBasePtr func = nullptr;
  void* data = nullptr;
  std::mutex lock;
  // FIXME: replace with double-linked lists.
  Vector<ResourceBase*> tracked;
  Vector<ResourceBase*> orphaned;
  Vector<ResourceBase*> orphanedCopy;
};

template<typename T>
class ResourceManager : public ResourceManagerBase
{
public:
  typedef void(*ReleaseFuncPtr)(void* data, T* resource);

  ResourceManager(ReleaseFuncPtr func, void* data) : ResourceManagerBase((ReleaseFuncBasePtr)func, data) {}

  ResourceHandle<T> createResource()
  {
    return ResourceHandle<T>(new T(*this));
  }
};

inline ResourceBase::ResourceBase(ResourceManagerBase& manager) : manager(manager) { manager.track(this); }
inline void ResourceBase::increment() { refs.fetch_add(1); }
inline void ResourceBase::decrement() { assert(refs); refs.fetch_sub(1); if (refs == 0) manager.orphan(this); }
