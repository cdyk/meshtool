#include "ResourceManager.h"
#include "Common.h"


void ResourceManagerBase::purge()
{
  if (trackedCount == 0) return;

  auto inUse = trackedCount;
  for (uint32_t i = 0; i < inUse; i++) {
    if (tracked[i]->refs == 0) {
      std::swap(tracked[i], tracked[inUse - 1]);
      inUse--;
    }
  }
}


void ResourceManagerBase::track(ResourceBase* resource)
{
  if (trackedReserved < trackedCount + 1) {
    trackedReserved = 2 * trackedReserved;
    if (trackedReserved < 16) trackedReserved = 16;
    tracked = (ResourceBase**)xrealloc(tracked, sizeof(ResourceBase*)*trackedReserved);
  }
  tracked[trackedCount++] = resource;
}

ResourceBase* ResourceManagerBase::getPurgedBase()
{
  if (trackedCount && tracked[trackedCount - 1]->refs == 0) {
    return tracked[--trackedCount];
  }
  return nullptr;
}
