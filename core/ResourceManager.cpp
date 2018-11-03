#include "ResourceManager.h"
#include "Common.h"
#include <cassert>

void ResourceManagerBase::track(ResourceBase* resource)
{
  std::lock_guard<std::mutex> guard(lock);
  tracked.pushBack(resource);
}

void ResourceManagerBase::orphan(ResourceBase* resource)
{
  std::lock_guard<std::mutex> guard(lock);
  auto N = tracked.size();
  for (size_t i = 0; i < N; i++) {
    if (resource == tracked[i]) {
      orphaned.pushBack(resource);
      tracked[i] = tracked[N - 1];
      tracked.resize(N - 1);
      return;
    }
  }
  assert(false && "Resource was not tracked");
}


uint32_t ResourceManagerBase::getCount()
{
  std::lock_guard<std::mutex> guard(lock);
  return tracked.size32();
}

uint32_t ResourceManagerBase::getOrphanCount()
{
  std::lock_guard<std::mutex> guard(lock);
  return orphaned.size32();
}

void ResourceManagerBase::houseKeep()
{
  assert(orphanedCopy.size() == 0);
  {
    std::lock_guard<std::mutex> guard(lock);
    orphanedCopy.swap(orphaned);
  }
  for (auto orphan : orphanedCopy) {
    if (!orphan->hasFlag(ResourceBase::Flags::External) && func) {
      func(data, orphan);
    }
    delete orphan;
  }
  orphanedCopy.clear();


}


void ResourceManagerBase::getOrphansBase(Vector<ResourceBase*>* o)
{
  std::lock_guard<std::mutex> guard(lock);
  auto N = orphaned.size();
  o->resize(N);
  for (size_t i = 0; i < N; i++) {
    (*o)[i] = orphaned[i];
  }
  orphaned.resize(0);
}
