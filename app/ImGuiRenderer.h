#pragma once
#include "Common.h"
#include "VulkanResources.h"

class App;

class ImGuiRenderer
{
public:
  ImGuiRenderer(Logger logger, App* app) : logger(logger), app(app) {}

  void init();
  void startFrame();
  void recordRendering(CommandBufferHandle commandBuffer);
  void shutdown();


private:
  Logger logger = nullptr;
  App* app = nullptr;
};
