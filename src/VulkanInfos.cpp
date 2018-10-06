#include "VulkanInfos.h"

VulkanInfos::VulkanInfos()
{
  {
    auto & info = vertexInput.v32b;
    info.binding = 0;
    info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    info.stride = 32;
  }
  {
    auto & info = vertexInput.v3f_0_0b;
    info.binding = 0;
    info.location = 0;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 0;
  }
  {
    auto & info = vertexInput.v3f_1_12b;
    info.binding = 0;
    info.location = 1;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 12;
  }
  {
    auto & info = vertexInput.v2f_2_24b;
    info.binding = 0;
    info.location = 2;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 24;
  }
  
  {
    auto & info = pipelineRasterization.cullBackLine;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode = VK_POLYGON_MODE_FILL;
    info.cullMode = VK_CULL_MODE_BACK_BIT;
    info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;
    info.depthBiasEnable = VK_FALSE;
    info.lineWidth = 1.0f;
  }
  {
    auto & info = pipelineRasterization.cullBackLine;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode = VK_POLYGON_MODE_LINE;
    info.cullMode = VK_CULL_MODE_BACK_BIT;
    info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;
    info.depthBiasEnable = VK_FALSE;
    info.depthBiasConstantFactor = 0;
    info.depthBiasClamp = 0;
    info.depthBiasSlopeFactor = 0;
    info.lineWidth = 1.0f;
  }
  {
    auto & info = pipelineRasterization.cullBackDepthBias;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode = VK_POLYGON_MODE_FILL;
    info.cullMode = VK_CULL_MODE_BACK_BIT;
    info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;
    info.depthBiasEnable = VK_TRUE;
    info.depthBiasConstantFactor = 1;
    info.depthBiasClamp = 0;
    info.depthBiasSlopeFactor = 1;
    info.lineWidth = 1.0f;
  }



}