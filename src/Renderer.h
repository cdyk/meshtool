#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"

class Renderer
{
public:
  Renderer(Logger logger, VkDevice device);
  ~Renderer();

private:
  VkDevice device;

  Buffer<VkPipelineShaderStageCreateInfo> vanillaShader;

};