#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"

struct ShaderInputSpec {
  uint32_t* spv;
  size_t siz;
  VkShaderStageFlagBits stage;
};

struct RenderPass
{
  VkSemaphore imgAcqSem;

};

struct RenderShader
{
  Buffer<VkPipelineShaderStageCreateInfo> stageCreateInfo;
};

struct RenderBuffer
{
  VkBuffer buffer;
  VkDeviceMemory mem;
  size_t size;
};

struct RenderImage
{
  VkImage image;
  VkDeviceMemory mem;
  VkImageView view;
  VkFormat format;
};


struct VulkanContext
{
  VulkanContext() = delete;

  VulkanContext(Logger logger, VkPhysicalDevice physicalDevice, VkDevice device);
  ~VulkanContext();

  void buildShader(RenderShader& shader, ShaderInputSpec* spec, unsigned N);
  void destroyShader(RenderShader& shader);

  void buildPipeline(VkPipeline& pipeline,
                     VkDevice device,
                     Buffer<VkPipelineShaderStageCreateInfo>& shaders,
                     Buffer<VkVertexInputBindingDescription>& inputBind,
                     Buffer<VkVertexInputAttributeDescription>& inputAttrib,
                     VkPipelineLayout pipelineLayout,
                     VkRenderPass renderPass,
                     VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                     VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                     VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);

  RenderBuffer createBuffer(size_t initialSize, VkImageUsageFlags usageFlags);
  RenderBuffer createVertexBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); }

  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);

  void createImage(RenderImage& renderImage, uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat depthFormat);
  void destroyImage(RenderImage renderImage);

  void createDepthImage(RenderImage& renderImage, uint32_t w, uint32_t h, VkFormat depthFormat = VK_FORMAT_D32_SFLOAT) {
    createImage(renderImage, w, h, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthFormat);
  }

  void createFrameBuffers(Buffer<VkFramebuffer>& frameBuffers, VkRenderPass renderPass, VkImageView depthView, VkImageView* colorViews, uint32_t colorViewCount, uint32_t w, uint32_t h);
  void destroyFrameBuffers(Buffer<VkFramebuffer>& frameBuffers);

  Logger logger;
  VkPhysicalDevice physicalDevice;
  VkDevice device;

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;
};