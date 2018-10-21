#pragma once
#include <vulkan/vulkan.h>


struct VulkanInfos
{
  VulkanInfos();

  struct {   
    VkVertexInputBindingDescription v4b;
    VkVertexInputBindingDescription v32b;
    VkVertexInputAttributeDescription v3f_0_0b;
    VkVertexInputAttributeDescription v3f_1_12b;
    VkVertexInputAttributeDescription v2f_2_24b;
    VkVertexInputAttributeDescription v4u8_0b;
  } vertexInput;

  struct {
    VkPipelineRasterizationStateCreateInfo cullNoneSolid;
    VkPipelineRasterizationStateCreateInfo cullBack;
    VkPipelineRasterizationStateCreateInfo cullBackLine;
    VkPipelineRasterizationStateCreateInfo cullNoneLine;
    VkPipelineRasterizationStateCreateInfo cullBackDepthBias;
  } pipelineRasterization;

  struct {
    VkCommandBufferBeginInfo singleShot;

  } commandBuffer;

  struct {
    VkImageViewCreateInfo baseLevel2D;

  } imageView;

  struct {
    VkSamplerCreateInfo triLlinearRepeat;

  } samplers;



};

