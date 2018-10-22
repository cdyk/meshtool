#pragma once
#include "VulkanResources.h"

enum struct RenderResourceState
{
  Uninitialized,
  Loading,
  Ready
};

enum struct TextureSource
{
  Checker,
  ColorGradient
};


struct RenderTextureResource : ResourceBase
{
  RenderTextureResource(ResourceManagerBase& manager) : ResourceBase(manager) {}

  RenderResourceState state = RenderResourceState::Uninitialized;
  TextureSource source = TextureSource::Checker;
  RenderBufferHandle stagingBuffer;
  ImageHandle image;
  ImageViewHandle view;
};
typedef ResourceHandle<RenderTextureResource> RenderTextureHandle;

class VulkanContext;

class RenderTextureManager : ResourceManager<RenderTextureResource>
{
public:
  RenderTextureManager(VulkanContext* context);

  RenderTextureHandle loadTexture(TextureSource source);

  void startFrame();

private:
  VulkanContext* vCtx = nullptr;
};