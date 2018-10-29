#pragma once
#include "Common.h"
#include "VulkanResources.h"

class VulkanManager;

class Raycaster
{
public:
  Raycaster(Logger logger, VulkanManager* vulkanManager);
  ~Raycaster();
  void init();

private:
  Logger logger;
  VulkanManager* vulkanManager = nullptr;

  ShaderHandle shader;
  PipelineHandle pipeline;
};