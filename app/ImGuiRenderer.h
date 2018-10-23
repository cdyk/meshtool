#pragma once
#include "Common.h"
#include "VulkanResources.h"

class VulkanManager;

class ImGuiRenderer
{
public:
  ImGuiRenderer(Logger logger, VulkanManager* vulkanManager) : logger(logger), vulkanManager(vulkanManager) {}

  void init();
  void startFrame();
  void recordRendering(CommandBufferHandle commandBuffer);
  void shutdown();


private:
  Logger logger = nullptr;
  VulkanManager* vulkanManager = nullptr;
};
