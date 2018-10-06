#pragma once
#include <vulkan/vulkan.h>


struct VulkanInfos
{
  VulkanInfos();

  struct {   
    VkVertexInputBindingDescription v32b;
    VkVertexInputAttributeDescription v3f_0_0b;
    VkVertexInputAttributeDescription v3f_1_12b;
    VkVertexInputAttributeDescription v2f_2_24b;
  } vertexInput;

  struct {
    VkPipelineRasterizationStateCreateInfo cullBack;
    VkPipelineRasterizationStateCreateInfo cullBackLine;
    VkPipelineRasterizationStateCreateInfo cullBackDepthBias;
  } pipelineRasterization;


};

