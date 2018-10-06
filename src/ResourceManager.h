#pragma once
#include <cassert>
#include <cstdint>

struct ResourceBase
{
  uint32_t refs = 0;



};

template<typename T>
struct ResourceHandle
{
  T* resource = nullptr;

  ResourceHandle() = default;
  ResourceHandle(ResourceHandle& rhs) { resource = rhs.resource; resource->refs++; }
  ResourceHandle(ResourceHandle&& rhs) { resource = rhs.resource; rhs.resource = nullptr; }
  ResourceHandle& operator=(ResourceHandle&& rhs) { resource = rhs.resource; rhs.resource = nullptr; }
  ~ResourceHandle() { release(); }


  ResourceHandle(T* resource)
    : resource(resource)
  {
    resource->refs++;
  }

  void release()
  {
    if (resource) {
      assert(resource->refs);
      resource->refs--;
      resource = nullptr;
    }
  }

};

class ResourceManagerBase
{
public:
  void purge();

protected:
  void track(ResourceBase* resource);

  ResourceBase* getPurgedBase();

private:
  ResourceBase** tracked = nullptr;
  uint32_t trackedCount = 0;
  uint32_t trackedReserved = 0;

};

template<typename T>
class ResourceManager : public ResourceManagerBase
{
public:
  ResourceHandle<T> createResource()
  {
    auto * r = new T();
    track(r);
    return ResourceHandle<T>(r);
  }

  T* getPurged() { return (T*)getPurgedBase(); }




};