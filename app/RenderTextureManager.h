#pragma once
#include "VulkanResources.h"
#include "RenderResource.h"

enum struct TextureSource
{
  Checker,
  ColorGradient
};


struct RenderTextureResource : RenderResourceBase
{
  RenderTextureResource(ResourceManagerBase& manager) : RenderResourceBase(manager) {}

  TextureSource source = TextureSource::Checker;
  RenderBufferHandle stagingBuffer;
  ImageHandle image;
  ImageViewHandle view;
};
typedef ResourceHandle<RenderTextureResource> RenderTextureHandle;

class VulkanContext;

class RenderTextureManager : public ResourceManager<RenderTextureResource>
{
public:
  RenderTextureManager(VulkanContext* context);

  RenderTextureHandle loadTexture(TextureSource source);

private:
  VulkanContext* vCtx = nullptr;
};