#include "VulkanInfos.h"

VulkanInfos::VulkanInfos()
{
  {
    auto & info = vertexInput.v4b;
    info = {};
    info.binding = 0;
    info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    info.stride = 4;
  }
  {
    auto & info = vertexInput.v32b;
    info = {};
    info.binding = 0;
    info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    info.stride = 32;
  }
  {
    auto & info = vertexInput.v3f_0_0b;
    info = {};
    info.binding = 0;
    info.location = 0;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 0;
  }
  {
    auto & info = vertexInput.v3f_1_12b;
    info = {};
    info.binding = 0;
    info.location = 1;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 12;
  }
  {
    auto & info = vertexInput.v2f_2_24b;
    info = {};
    info.binding = 0;
    info.location = 2;
    info.format = VK_FORMAT_R32G32B32_SFLOAT;
    info.offset = 24;
  }
  {
    auto & info = vertexInput.v4u8_0b;
    info = {};
    info.binding = 0;
    info.location = 3;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.offset = 0;
  }

  {
    auto & info = pipelineRasterization.cullNoneSolid;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode = VK_POLYGON_MODE_FILL;
    info.cullMode = VK_CULL_MODE_NONE;
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
    auto & info = pipelineRasterization.cullNoneLine;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.polygonMode = VK_POLYGON_MODE_LINE;
    info.cullMode = VK_CULL_MODE_NONE;
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
  {
    auto & info = commandBuffer.singleShot;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  }
  {
    auto & info = imageView.baseLevel2D;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.image = VK_NULL_HANDLE;
    info.format = VK_FORMAT_UNDEFINED;
    info.components.r = VK_COMPONENT_SWIZZLE_R;
    info.components.g = VK_COMPONENT_SWIZZLE_G;
    info.components.b = VK_COMPONENT_SWIZZLE_B;
    info.components.a = VK_COMPONENT_SWIZZLE_A;
    info.subresourceRange.aspectMask = 0;// VK_IMAGE_ASPECT_DEPTH_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.flags = 0;
    info.image = VK_NULL_HANDLE;

  }

  {
    auto & info = samplers.triLlinearRepeat;
    info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = 16;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;
  }

}