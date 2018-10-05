#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"

struct VulkanContext;
struct RenderMesh;
struct RenderPipeline;
struct RenderPass;
struct RenderImage;

class Renderer
{
public:
  Renderer(Logger logger, VkPhysicalDevice physicalDevice, VkDevice device, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h);
  ~Renderer();

  RenderMesh* createRenderMesh(Mesh* mesh);

  void drawRenderMesh(RenderMesh* renderMesh);
  void destroyRenderMesh(RenderMesh* renderMesh);

private:
  Logger logger;
  VulkanContext* vkCtx = nullptr;
  RenderPipeline* vanillaPipeline = nullptr;

  RenderImage* depthImage = nullptr;

  //VkImage depthImage;
  //VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

};