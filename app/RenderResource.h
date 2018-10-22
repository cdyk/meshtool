#pragma once
#include "ResourceManager.h"

enum struct RenderResourceState
{
  Uninitialized,
  Loading,
  Ready
};

struct RenderResourceBase : ResourceBase
{
  RenderResourceBase(ResourceManagerBase& manager) : ResourceBase(manager) {}

  RenderResourceState state = RenderResourceState::Uninitialized;

};