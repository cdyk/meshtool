#include <cassert>
#include <vulkan/spirv.h>
#include "VulkanContext.h"
#include "VulkanResources.h"


namespace {

  void destroyBuffer(void* data, Buffer* buffer)
  {
    auto device = ((VulkanContext*)data)->device;
    if (buffer->hostPtr) vkUnmapMemory(device, buffer->mem);
    if (buffer->buffer) vkDestroyBuffer(device, buffer->buffer, nullptr);
    if (buffer->mem) vkFreeMemory(device, buffer->mem, nullptr);
  }

  void destroyDescriptorSet(void* data, DescriptorSet* descSet)
  {
    auto device = ((VulkanContext*)data)->device;
    auto descPool = ((VulkanContext*)data)->descPool;
    if (descSet->descSet) vkFreeDescriptorSets(device, descPool, 1, &descSet->descSet);
  }

  void destroyDescriptorPool(void* data, DescriptorPool* pool)
  {
    if (pool) {
      auto device = ((VulkanContext*)data)->device;
      auto descPool = ((VulkanContext*)data)->descPool;
      vkDestroyDescriptorPool(device, pool->pool, nullptr);
      pool->pool = nullptr;
    }
  }

  void destroyShader(void* data, Shader* shader)
  {
    auto device = ((VulkanContext*)data)->device;
    if (shader->stageCreateInfo.module) {
      vkDestroyShaderModule(device, shader->stageCreateInfo.module, nullptr);
    }
    shader->stageCreateInfo.module = nullptr;
  }

  void destroyPipeline(void* data, Pipeline* pipe)
  {
    auto device = ((VulkanContext*)data)->device;
    if (pipe->pipe) vkDestroyPipeline(device, pipe->pipe, nullptr);
    if (pipe->pipeLayout) vkDestroyPipelineLayout(device, pipe->pipeLayout, nullptr);
    if (pipe->descLayout) vkDestroyDescriptorSetLayout(device, pipe->descLayout, nullptr);
  }

  void destroyImage(void* data, Image* image)
  {
    auto device = ((VulkanContext*)data)->device;
    if (image->image)vkDestroyImage(device, image->image, nullptr);
    if (image->mem) vkFreeMemory(device, image->mem, nullptr);
  }

  void destroyImageView(void* data, ImageView* view)
  {
    auto device = ((VulkanContext*)data)->device;
    if (view->view) vkDestroyImageView(device, view->view, nullptr);
  }

  void destroySampler(void* data, Sampler* sampler)
  {
    auto device = ((VulkanContext*)data)->device;
    if (sampler->sampler) vkDestroySampler(device, sampler->sampler, nullptr);
  }

  void destroyRenderPass(void* data, RenderPass* pass)
  {
    auto device = ((VulkanContext*)data)->device;
    if (pass->pass) vkDestroyRenderPass(device, pass->pass, nullptr);
    pass->pass = nullptr;
  }

  void destroyFrameBuffer(void* data, FrameBuffer* fb)
  {
    auto device = ((VulkanContext*)data)->device;
    if (fb->fb) vkDestroyFramebuffer(device, fb->fb, nullptr);
  }

  void destroyCommandPool(void* data, CommandPool* cmdPool)
  {
    auto device = ((VulkanContext*)data)->device;
    if (cmdPool->cmdPool) vkDestroyCommandPool(device, cmdPool->cmdPool, nullptr);
    cmdPool->cmdPool = nullptr;
  }

  void destroyFence(void* data, Fence* fence)
  {
    auto device = ((VulkanContext*)data)->device;
    if (fence->fence) vkDestroyFence(device, fence->fence, nullptr);
    fence->fence = VK_NULL_HANDLE;
  }

  void destroySemaphore(void* data, Semaphore* semaphore)
  {
    auto device = ((VulkanContext*)data)->device;
    vkDestroySemaphore(device, semaphore->semaphore, nullptr);
    semaphore->semaphore = nullptr;
  }

  void destroySwapChain(void* data, SwapChain* swapChain)
  {
    auto device = ((VulkanContext*)data)->device;
    vkDestroySwapchainKHR(device, swapChain->swapChain, nullptr);
  }

  void destroyAccelerationStructure(void* data, AccelerationStructure* accStr)
  {
    auto* vCtx = (VulkanContext*)data;
    if (accStr->acc) {
      vCtx->vkDestroyAccelerationStructureNV(vCtx->device, accStr->acc, nullptr);
      accStr->acc = VK_NULL_HANDLE;
    }
    if (accStr->structureMem) {
      vkFreeMemory(vCtx->device, accStr->structureMem, nullptr);
      accStr->structureMem = VK_NULL_HANDLE;
    }
    ((VulkanContext*)data)->logger(0, "Destroyed acceleration structure.");
  }

}



void VulkanResources::init()
{
}

VulkanResources::VulkanResources(VulkanContext* vCtx, Logger logger) :
  vCtx(vCtx),
  logger(logger),
  bufferResources(destroyBuffer, vCtx),
  descriptorSetResources(destroyDescriptorSet, vCtx),
  descriptorPoolResources(destroyDescriptorPool, vCtx),
  shaderResources(destroyShader, vCtx),
  pipelineResources(destroyPipeline, vCtx),
  renderPassResources(destroyRenderPass, vCtx),
  frameBufferResources(destroyFrameBuffer, vCtx),
  renderImageResources(destroyImage, vCtx),
  imageViewResources(destroyImageView, vCtx),
  samplerResources(destroySampler, vCtx),
  commandPoolResources(destroyCommandPool, vCtx),
  fenceResources(destroyFence, vCtx),
  semaphoreResources(destroySemaphore, vCtx),
  swapChainResources(destroySwapChain, vCtx),
  accelerationStructures(destroyAccelerationStructure, vCtx)
{}


VulkanResources::~VulkanResources()
{
  houseKeep();
  logger(0, "%d unreleased buffers", bufferResources.getCount());
  logger(0, "%d unreleased descriptors", descriptorSetResources.getCount());
  logger(0, "%d unreleased shaders", shaderResources.getCount());
  logger(0, "%d unreleased pipelines", pipelineResources.getCount());
  logger(0, "%d unreleased renderPasses", renderPassResources.getCount());
  logger(0, "%d unreleased frameBuffers", frameBufferResources.getCount());
  logger(0, "%d unreleased renderImages", renderImageResources.getCount());
  logger(0, "%d unreleased commandPoolResources", commandPoolResources.getCount());
  logger(0, "%d unreleased fenceResources", fenceResources.getCount());
  logger(0, "%d unreleased semaphoreResources", semaphoreResources.getCount());
  logger(0, "%d unreleased swapChainResources", swapChainResources.getCount());
}

void VulkanResources::copyHostMemToBuffer(RenderBufferHandle buffer, void* src, size_t size)
{
  assert(buffer.resource->hostPtr);
  std::memcpy(buffer.resource->hostPtr, src, size);
}


void VulkanResources::houseKeep()
{
  bufferResources.houseKeep();
  descriptorSetResources.houseKeep();
  shaderResources.houseKeep();
  pipelineResources.houseKeep();
  renderPassResources.houseKeep();
  frameBufferResources.houseKeep();
  renderImageResources.houseKeep();
  imageViewResources.houseKeep();
  samplerResources.houseKeep();
  commandPoolResources.houseKeep();
  fenceResources.houseKeep();
  semaphoreResources.houseKeep();
  swapChainResources.houseKeep();
  accelerationStructures.houseKeep();
}

PipelineHandle VulkanResources::createPipeline()
{
  return pipelineResources.createResource();
}


PipelineHandle VulkanResources::createPipeline(const Vector<VkVertexInputBindingDescription>& inputBind,
                                               const Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                               VkPipelineLayoutCreateInfo& pipelineLayoutInfo_,
                                               VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                               RenderPassHandle renderPass,
                                               Vector<ShaderHandle> shaders,
                                               const VkPipelineRasterizationStateCreateInfo& rasterizationInfo,
                                               VkPrimitiveTopology primitiveTopology)
{
  auto pipeHandle = pipelineResources.createResource();
  auto * pipe = pipeHandle.resource;

  pipe->pass = renderPass;

  VkDynamicState dynamicStateEnables[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  auto rv = vkCreateDescriptorSetLayout(vCtx->device, &descLayoutInfo, nullptr, &pipe->descLayout);
  assert(rv == VK_SUCCESS);

  VkPipelineLayoutCreateInfo& pipelineLayoutInfo = pipelineLayoutInfo_;
  pipelineLayoutInfo.setLayoutCount = 1;// uint32_t(descSetLayout.getCount());
  pipelineLayoutInfo.pSetLayouts = &pipe->descLayout;
  rv = vkCreatePipelineLayout(vCtx->device, &pipelineLayoutInfo, nullptr, &pipe->pipeLayout);
  assert(rv == VK_SUCCESS);

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pNext = nullptr;
  dynamicState.pDynamicStates = dynamicStateEnables;
  dynamicState.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(VkDynamicState);

  VkPipelineVertexInputStateCreateInfo vtxInputCI;
  vtxInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vtxInputCI.pNext = nullptr;
  vtxInputCI.flags = 0;
  vtxInputCI.vertexBindingDescriptionCount = uint32_t(inputBind.size());
  vtxInputCI.pVertexBindingDescriptions = inputBind.data();
  vtxInputCI.vertexAttributeDescriptionCount = uint32_t(inputAttrib.size());
  vtxInputCI.pVertexAttributeDescriptions = inputAttrib.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI;
  inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyCI.pNext = nullptr;
  inputAssemblyCI.flags = 0;
  inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
  inputAssemblyCI.topology = primitiveTopology;

  VkPipelineColorBlendAttachmentState blendAttachmentState[1];
  blendAttachmentState[0].colorWriteMask = 0xf;
  blendAttachmentState[0].blendEnable = VK_FALSE;
  blendAttachmentState[0].alphaBlendOp = VK_BLEND_OP_ADD;
  blendAttachmentState[0].colorBlendOp = VK_BLEND_OP_ADD;
  blendAttachmentState[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachmentState[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachmentState[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachmentState[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

  VkPipelineColorBlendStateCreateInfo blendCI;
  blendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blendCI.pNext = nullptr;
  blendCI.flags = 0;
  blendCI.attachmentCount = 1;
  blendCI.pAttachments = blendAttachmentState;
  blendCI.logicOpEnable = VK_FALSE;
  blendCI.logicOp = VK_LOGIC_OP_NO_OP;
  blendCI.blendConstants[0] = 1.0f;
  blendCI.blendConstants[1] = 1.0f;
  blendCI.blendConstants[2] = 1.0f;
  blendCI.blendConstants[3] = 1.0f;

  VkPipelineMultisampleStateCreateInfo multiSampleCI;
  multiSampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multiSampleCI.pNext = nullptr;
  multiSampleCI.flags = 0;
  multiSampleCI.pSampleMask = nullptr;
  multiSampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multiSampleCI.sampleShadingEnable = VK_FALSE;
  multiSampleCI.alphaToCoverageEnable = VK_FALSE;
  multiSampleCI.alphaToOneEnable = VK_FALSE;
  multiSampleCI.minSampleShading = 0.0;

  VkPipelineDepthStencilStateCreateInfo depthStencilCI;
  depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilCI.pNext = nullptr;
  depthStencilCI.flags = 0;
  depthStencilCI.depthTestEnable = VK_TRUE;
  depthStencilCI.depthWriteEnable = VK_TRUE;
  depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencilCI.depthBoundsTestEnable = VK_FALSE;
  depthStencilCI.minDepthBounds = 0;
  depthStencilCI.maxDepthBounds = 0;
  depthStencilCI.stencilTestEnable = VK_FALSE;
  depthStencilCI.back.failOp = VK_STENCIL_OP_KEEP;
  depthStencilCI.back.passOp = VK_STENCIL_OP_KEEP;
  depthStencilCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
  depthStencilCI.back.compareMask = 0;
  depthStencilCI.back.reference = 0;
  depthStencilCI.back.depthFailOp = VK_STENCIL_OP_KEEP;
  depthStencilCI.back.writeMask = 0;
  depthStencilCI.front = depthStencilCI.back;

  VkPipelineViewportStateCreateInfo viewportCI = {};
  viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportCI.pNext = nullptr;
  viewportCI.flags = 0;
  viewportCI.viewportCount = 1;
  viewportCI.scissorCount = 1;
  viewportCI.pScissors = nullptr;
  viewportCI.pViewports = nullptr;


  Vector<VkPipelineShaderStageCreateInfo> stages(shaders.size());
  for (size_t i = 0; i < shaders.size(); i++) {
    stages[i] = shaders[i].resource->stageCreateInfo;
  }

  VkGraphicsPipelineCreateInfo pipelineCI;
  pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCI.pNext = nullptr;
  pipelineCI.layout =  pipe->pipeLayout;
  pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
  pipelineCI.basePipelineIndex = 0;
  pipelineCI.flags = 0;
  pipelineCI.pVertexInputState = &vtxInputCI;
  pipelineCI.pInputAssemblyState = &inputAssemblyCI;
  pipelineCI.pRasterizationState = &rasterizationInfo;
  pipelineCI.pColorBlendState = &blendCI;
  pipelineCI.pTessellationState = nullptr;
  pipelineCI.pMultisampleState = &multiSampleCI;
  pipelineCI.pDynamicState = &dynamicState;
  pipelineCI.pViewportState = &viewportCI;
  pipelineCI.pDepthStencilState = &depthStencilCI;
  pipelineCI.pStages = stages.data();
  pipelineCI.stageCount = stages.size32();
  pipelineCI.renderPass = renderPass.resource->pass;
  pipelineCI.subpass = 0;

  rv = vkCreateGraphicsPipelines(vCtx->device, vCtx->pipelineCache, 1, &pipelineCI, nullptr, &pipe->pipe);
  assert(rv == VK_SUCCESS);

  logger(0, "Built pipeline");
  return pipeHandle;
}

DescriptorPoolHandle VulkanResources::createDescriptorPool(const VkDescriptorPoolCreateInfo* info)
{
  auto handle = descriptorPoolResources.createResource();
  auto device = vCtx->device;
  CHECK_VULKAN(vkCreateDescriptorPool(device, info, nullptr, &handle.resource->pool));
  return handle;
}

RenderBufferHandle VulkanResources::createBuffer()
{
  return bufferResources.createResource();
}

RenderBufferHandle VulkanResources::createBuffer(size_t requestedSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags properties)
{
  auto device = vCtx->device;
  auto bufHandle = bufferResources.createResource();
  auto * buf = bufHandle.resource;
  buf->requestedSize = requestedSize;

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.pNext = nullptr;
  bufferCI.usage = usageFlags;
  bufferCI.size = requestedSize;
  bufferCI.queueFamilyIndexCount = 0;
  bufferCI.pQueueFamilyIndices = nullptr;
  bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCI.flags = 0;
  CHECK_VULKAN(vkCreateBuffer(device, &bufferCI, nullptr, &buf->buffer));

  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(vCtx->device, buf->buffer, &memReqs);
  buf->alignedSize = memReqs.size;

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = NULL;
  allocInfo.memoryTypeIndex = 0;
  allocInfo.allocationSize = memReqs.size;
  CHECK_BOOL(getMemoryTypeIndex(allocInfo.memoryTypeIndex, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
  CHECK_VULKAN(vkAllocateMemory(device, &allocInfo, NULL, &buf->mem));
  CHECK_VULKAN(vkBindBufferMemory(device, buf->buffer, buf->mem, 0));

  buf->descInfo.buffer = buf->buffer;
  buf->descInfo.offset = 0;
  buf->descInfo.range = VK_WHOLE_SIZE;

  if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    CHECK_VULKAN(vkMapMemory(device, buf->mem, 0, buf->alignedSize, 0, &buf->hostPtr));
  }

  return bufHandle;
}

DescriptorSetHandle VulkanResources::createDescriptorSet(VkDescriptorSetLayout descLayout)
{
  auto descSetHandle = descriptorSetResources.createResource();
  auto * descSet = descSetHandle.resource;

  VkDescriptorSetAllocateInfo alloc_info[1];
  alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info[0].pNext = nullptr;
  alloc_info[0].descriptorPool = vCtx->descPool;
  alloc_info[0].descriptorSetCount = 1;
  alloc_info[0].pSetLayouts = &descLayout;

  auto rv = vkAllocateDescriptorSets(vCtx->device, alloc_info, &descSet->descSet);
  assert(rv == VK_SUCCESS);

  return descSetHandle;
}

namespace {

  VkShaderStageFlagBits getShaderStage(SpvExecutionModel executionModel)
  {
    switch (executionModel) {
    case SpvExecutionModelVertex: return VK_SHADER_STAGE_VERTEX_BIT;
    case SpvExecutionModelTessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case SpvExecutionModelTessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case SpvExecutionModelGeometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case SpvExecutionModelFragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
      //case SpvExecutionModelGLCompute: return VK_SHADER_STAGE_;
      //case SpvExecutionModelKernel: return VK_SHADER_STAGE;
    case SpvExecutionModelTaskNV: return VK_SHADER_STAGE_TASK_BIT_NV;
    case SpvExecutionModelMeshNV: return VK_SHADER_STAGE_MESH_BIT_NV;
    case SpvExecutionModelRayGenerationNV: return VK_SHADER_STAGE_RAYGEN_BIT_NV;
    case SpvExecutionModelIntersectionNV: return VK_SHADER_STAGE_INTERSECTION_BIT_NV;
    case SpvExecutionModelAnyHitNV: return VK_SHADER_STAGE_ANY_HIT_BIT_NV;
    case SpvExecutionModelClosestHitNV: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
    case SpvExecutionModelMissNV: return VK_SHADER_STAGE_MISS_BIT_NV;
    case SpvExecutionModelCallableNV: return VK_SHADER_STAGE_CALLABLE_BIT_NV;
    default:
      assert(false && "Unsupported execution model");
      return (VkShaderStageFlagBits)0;
    }
  }

  void introspect(Shader* shader, const uint32_t*spv, const size_t words)
  {
    assert(5 <= words);
    assert(spv[0] == SpvMagicNumber);

    auto idBound = spv[3];
    auto * instruction = spv + 5;
    while (instruction != spv + words) {
      auto opCode = instruction[0] & 0xffffu;
      auto wordCount = (instruction[0] >> 16) & 0xffffu;
      switch (opCode) {
      case SpvOpEntryPoint:
        assert(2 <= wordCount);
        shader->stageCreateInfo.stage = getShaderStage(SpvExecutionModel(instruction[1]));
        break;
      }
      assert(instruction + wordCount <= spv + words);
      instruction += wordCount;
    }
  }

}


ShaderHandle VulkanResources::createShader(uint32_t* spv, size_t siz)
{
  auto device = vCtx->device;

  auto shaderHandle = shaderResources.createResource();
  auto * shader = shaderHandle.resource;

  auto & stageInfo = shader->stageCreateInfo;
  stageInfo.pName = "main";

  introspect(shader, spv, siz / sizeof(uint32_t));
  assert(stageInfo.stage != 0);

  VkShaderModuleCreateInfo moduleCreateInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
  moduleCreateInfo.codeSize = siz;
  moduleCreateInfo.pCode = spv;
  CHECK_VULKAN(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &stageInfo.module));

  return shaderHandle;
}

ImageHandle VulkanResources::createImage(VkImageCreateInfo& imageCreateInfo)
{
  auto renderImageHandle = renderImageResources.createResource();
  auto * renderImage = renderImageHandle.resource;
  renderImage->format = imageCreateInfo.format;

  auto rv = vkCreateImage(vCtx->device, &imageCreateInfo, nullptr, &renderImage->image);
  assert(rv == VK_SUCCESS);

  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(vCtx->device, renderImage->image, &memReqs);

  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.pNext = nullptr;
  //memAllocInfo.allocationSize = 0;
  //memAllocInfo.memoryTypeIndex = 0;
  memAllocInfo.allocationSize = memReqs.size;

  auto rvb = getMemoryTypeIndex(memAllocInfo.memoryTypeIndex, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  assert(rvb);

  rv = vkAllocateMemory(vCtx->device, &memAllocInfo, NULL, &renderImage->mem);
  assert(rv == VK_SUCCESS);

  rv = vkBindImageMemory(vCtx->device, renderImage->image, renderImage->mem, 0);
  assert(rv == VK_SUCCESS);

  

  return renderImageHandle;
}

ImageHandle VulkanResources::createImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format)
{
  VkImageCreateInfo imageCI = {};
  imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCI.pNext = nullptr;
  imageCI.imageType = VK_IMAGE_TYPE_2D;
  imageCI.format = format;
  imageCI.extent.width = w;
  imageCI.extent.height = h;
  imageCI.extent.depth = 1;
  imageCI.mipLevels = 1;
  imageCI.arrayLayers = 1;
  imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCI.usage = usageFlags;
  imageCI.queueFamilyIndexCount = 0;
  imageCI.pQueueFamilyIndices = nullptr;
  imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCI.flags = 0;

  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(vCtx->physicalDevice, format, &props);
  if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    imageCI.tiling = VK_IMAGE_TILING_LINEAR;
  }
  else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  }
  else {
    assert(false && "depth format unsupported");
  }

  auto renderImageHandle = createImage(imageCI);
  auto * renderImage = renderImageHandle.resource;

  return renderImageHandle;
}

ImageViewHandle VulkanResources::createImageView(ImageHandle image, VkImageViewCreateInfo& imageViewCreateInfo)
{
  auto view = imageViewResources.createResource();
  view.resource->image = image;

  VkImageViewCreateInfo info = imageViewCreateInfo;
  info.format = image.resource->format;
  info.image = image.resource->image;
  auto rv = vkCreateImageView(vCtx->device, &info, NULL, &view.resource->view);
  assert(rv == VK_SUCCESS);

  return view;
}

ImageViewHandle VulkanResources::createImageView(VkImage image, VkFormat format, VkImageSubresourceRange imageRange)
{
  auto view = imageViewResources.createResource();

  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = format;
  info.components.r = VK_COMPONENT_SWIZZLE_R;
  info.components.g = VK_COMPONENT_SWIZZLE_G;
  info.components.b = VK_COMPONENT_SWIZZLE_B;
  info.components.a = VK_COMPONENT_SWIZZLE_A;
  info.image = image;
  info.subresourceRange = imageRange;

  auto rv = vkCreateImageView(vCtx->device, &info, nullptr, &view.resource->view);
  assert(rv == VK_SUCCESS);

  return view;
}

SamplerHandle VulkanResources::createSampler(VkSamplerCreateInfo& samplerCreateInfo)
{
  auto sampler = samplerResources.createResource();
  auto rv = vkCreateSampler(vCtx->device, &samplerCreateInfo, nullptr, &sampler.resource->sampler);
  assert(rv == VK_SUCCESS);
  return sampler;
}

RenderPassHandle VulkanResources::createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                                         VkSubpassDescription* subpasses, uint32_t subpassCount,
                                                         VkSubpassDependency* dependency)
{
  auto passHandle = renderPassResources.createResource();
  auto * pass = passHandle.resource;

  {
    VkRenderPassCreateInfo rp_info = {};

    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = attachmentCount;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = subpassCount;
    rp_info.pSubpasses = subpasses;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = dependency;

    auto rv = vkCreateRenderPass(vCtx->device, &rp_info, NULL, &pass->pass);
    assert(rv == VK_SUCCESS);
  }

  return passHandle;
}

FrameBufferHandle VulkanResources::createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<ImageViewHandle>& attachments)
{
  auto fbHandle = frameBufferResources.createResource();
  auto * fb = fbHandle.resource;

  fb->pass = pass;

  MemBuffer<VkImageView> att(attachments.size());
  fb->attachments.resize(attachments.size());
  for (size_t i = 0; i < attachments.size(); i++) {
    fb->attachments[i] = attachments[i];
    att[i] = attachments[i].resource->view;
  }

  VkFramebufferCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  info.pNext = NULL;
  info.renderPass = pass.resource->pass;
  info.attachmentCount = uint32_t(attachments.size());
  info.pAttachments = att.data();
  info.width = w;
  info.height = h;
  info.layers = 1;
  auto rv = vkCreateFramebuffer(vCtx->device, &info, NULL, &fb->fb);
  assert(rv == VK_SUCCESS);

  return fbHandle;
}

CommandPoolHandle VulkanResources::createCommandPool(uint32_t queueFamilyIx)
{
  VkCommandPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  info.queueFamilyIndex = queueFamilyIx;

  auto poolHandle = commandPoolResources.createResource();
  auto rv = vkCreateCommandPool(vCtx->device, &info, nullptr, &poolHandle.resource->cmdPool);
  assert(rv == VK_SUCCESS);
  return poolHandle;
}


FenceHandle VulkanResources::createFence(bool signalled)
{
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

  auto handle = fenceResources.createResource();
  auto rv = vkCreateFence(vCtx->device, &info, nullptr, &handle.resource->fence);
  assert(rv == VK_SUCCESS);
  return handle;
}

SemaphoreHandle VulkanResources::createSemaphore()
{
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  auto handle = semaphoreResources.createResource();
  auto rv = vkCreateSemaphore(vCtx->device, &info, nullptr, &handle.resource->semaphore);
  assert(rv == VK_SUCCESS);
  return handle;
}

SwapChainHandle VulkanResources::createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo)
{
  auto info = swapChainInfo;
  info.oldSwapchain = oldSwapChain ? oldSwapChain.resource->swapChain : nullptr;

  auto handle = swapChainResources.createResource();
  CHECK_VULKAN(vkCreateSwapchainKHR(vCtx->device, &info, nullptr, &handle.resource->swapChain));

  return handle;
}


AccelerationStructureHandle VulkanResources::createAccelerationStructure()
{
  return accelerationStructures.createResource();
}

AccelerationStructureHandle VulkanResources::createAccelerationStructure(VkAccelerationStructureTypeNV type, uint32_t geometryCount, VkGeometryNV* geometries, uint32_t instanceCount)
{
  auto handle = accelerationStructures.createResource();
  auto * r = handle.resource;
  //VkAccelerationStructureNVX acc = VK_NULL_HANDLE;
  //VkDeviceMemory structureMem = VK_NULL_HANDLE;
  //VkDeviceMemory scratchMem = VK_NULL_HANDLE;
  //VkBuffer scratchBuffer = VK_NULL_HANDLE;

  VkAccelerationStructureCreateInfoNV asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
  asInfo.compactedSize = 0;
  asInfo.info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
  asInfo.info.type = type;
  asInfo.info.flags = 0;
  asInfo.info.instanceCount = instanceCount;
  asInfo.info.geometryCount = geometryCount;
  asInfo.info.pGeometries = geometries;
  CHECK_VULKAN(vCtx->vkCreateAccelerationStructureNV(vCtx->device, &asInfo, nullptr, &r->acc));

  VkMemoryRequirements2 memoryRequirements;
  {
    VkAccelerationStructureMemoryRequirementsInfoNV mrInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
    mrInfo.accelerationStructure = r->acc;
    mrInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
    vCtx->vkGetAccelerationStructureMemoryRequirementsNV(vCtx->device, &mrInfo, &memoryRequirements);
  }

  VkMemoryAllocateInfo memoryAllocateInfo;
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.pNext = nullptr;
  memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  CHECK_VULKAN(vkAllocateMemory(vCtx->device, &memoryAllocateInfo, nullptr, &r->structureMem));

  VkBindAccelerationStructureMemoryInfoNV bindInfo{ VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV };
  bindInfo.accelerationStructure = r->acc;
  bindInfo.memory = r->structureMem;
  bindInfo.memoryOffset = 0;
  bindInfo.deviceIndexCount = 0;
  bindInfo.pDeviceIndices = nullptr;
  CHECK_VULKAN(vCtx->vkBindAccelerationStructureMemoryNV(vCtx->device, 1, &bindInfo));

  VkMemoryRequirements2 scratchMemReq;
  {
    VkAccelerationStructureMemoryRequirementsInfoNV mrInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV };
    mrInfo.accelerationStructure = r->acc;
    mrInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
    vCtx->vkGetAccelerationStructureMemoryRequirementsNV(vCtx->device, &mrInfo, &scratchMemReq);
  }
  r->scratchReqs = scratchMemReq.memoryRequirements;

  return handle;
}

bool VulkanResources::getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements)
{
  for (uint32_t i = 0; i < vCtx->memoryProperties.memoryTypeCount; i++) {
    auto bit = 1 << i;
    if (typeBits & bit) {
      if ((vCtx->memoryProperties.memoryTypes[i].propertyFlags & requirements) == requirements) {
        index = i;
        return true;
      }
    }
  }
  return false;
}

uint32_t VulkanResources::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags requirements)
{
  for (uint32_t i = 0; i < vCtx->memoryProperties.memoryTypeCount; i++) {
    auto bit = 1 << i;
    if (typeBits & bit) {
      if ((vCtx->memoryProperties.memoryTypes[i].propertyFlags & requirements) == requirements) {
        return i;
      }
    }
  }
  assert(!"Failed to find suitable memory.");
  return ~0;
}
