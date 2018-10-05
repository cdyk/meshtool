#include <cassert>
#include "VulkanContext.h"


VulkanContext::VulkanContext(Logger logger, VkPhysicalDevice physicalDevice, VkDevice device) :
  logger(logger),
  physicalDevice(physicalDevice),
  device(device)
{
  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

VulkanContext::~VulkanContext()
{

}

//(/void VulkanContext::buildShaders(Buffer<VkPipelineShaderStageCreateInfo>& stageCreateInfo,
//                                 ShaderInputSpec* spec, unsigned N)

void VulkanContext::buildShader(RenderShader& shader, ShaderInputSpec* spec, unsigned N)
{
  shader.stageCreateInfo.accommodate(N);
  for (unsigned i = 0; i < 2; i++) {
    shader.stageCreateInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader.stageCreateInfo[i].pNext = nullptr;
    shader.stageCreateInfo[i].pSpecializationInfo = nullptr;
    shader.stageCreateInfo[i].flags = 0;
    shader.stageCreateInfo[i].pName = "main";
    shader.stageCreateInfo[i].stage = spec[i].stage;

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = nullptr;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = spec[i].siz;
    moduleCreateInfo.pCode = spec[i].spv;
    auto rv = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shader.stageCreateInfo[i].module);
    assert(rv == VK_SUCCESS);
  }
}

void VulkanContext::destroyShader(RenderShader& shader)
{
  for (size_t i = 0; i < shader.stageCreateInfo.getCount(); i++) {
    vkDestroyShaderModule(device, shader.stageCreateInfo[i].module, nullptr);
  }
}


void VulkanContext::buildPipeline(VkPipeline& pipeline,
                                  VkDevice device,
                                  Buffer<VkPipelineShaderStageCreateInfo>& shaders,
                                  Buffer<VkVertexInputBindingDescription>& inputBind,
                                  Buffer<VkVertexInputAttributeDescription>& inputAttrib,
                                  VkPipelineLayout pipelineLayout,
                                  VkRenderPass renderPass,
                                  VkPrimitiveTopology primitiveTopology,
                                  VkCullModeFlags cullMode,
                                  VkPolygonMode polygonMode)
{
  VkDynamicState dynamicStateEnables[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pNext = nullptr;
  dynamicState.pDynamicStates = dynamicStateEnables;
  dynamicState.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(VkDynamicState);

  VkPipelineVertexInputStateCreateInfo vtxInputCI;
  vtxInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vtxInputCI.pNext = nullptr;
  vtxInputCI.flags = 0;
  vtxInputCI.vertexBindingDescriptionCount = uint32_t(inputBind.getCount());
  vtxInputCI.pVertexBindingDescriptions = inputBind.data();
  vtxInputCI.vertexAttributeDescriptionCount = uint32_t(inputAttrib.getCount());
  vtxInputCI.pVertexAttributeDescriptions = inputAttrib.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI;
  inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyCI.pNext = nullptr;
  inputAssemblyCI.flags = 0;
  inputAssemblyCI.primitiveRestartEnable = VK_FALSE;
  inputAssemblyCI.topology = primitiveTopology;

  VkPipelineRasterizationStateCreateInfo rasterizationCI;
  rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationCI.pNext = nullptr;
  rasterizationCI.flags = 0;
  rasterizationCI.polygonMode = polygonMode;
  rasterizationCI.cullMode = cullMode;
  rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationCI.depthClampEnable = VK_FALSE;
  rasterizationCI.rasterizerDiscardEnable = VK_FALSE;
  rasterizationCI.depthBiasEnable = VK_FALSE;
  rasterizationCI.depthBiasConstantFactor = 0;
  rasterizationCI.depthBiasClamp = 0;
  rasterizationCI.depthBiasSlopeFactor = 0;
  rasterizationCI.lineWidth = 1.0f;

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

  VkGraphicsPipelineCreateInfo pipelineCI;
  pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCI.pNext = nullptr;
  pipelineCI.layout = pipelineLayout;
  pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
  pipelineCI.basePipelineIndex = 0;
  pipelineCI.flags = 0;
  pipelineCI.pVertexInputState = &vtxInputCI;
  pipelineCI.pInputAssemblyState = &inputAssemblyCI;
  pipelineCI.pRasterizationState = &rasterizationCI;
  pipelineCI.pColorBlendState = &blendCI;
  pipelineCI.pTessellationState = nullptr;
  pipelineCI.pMultisampleState = &multiSampleCI;
  pipelineCI.pDynamicState = &dynamicState;
  pipelineCI.pViewportState = &viewportCI;
  pipelineCI.pDepthStencilState = &depthStencilCI;
  pipelineCI.pStages = shaders.data();
  pipelineCI.stageCount = uint32_t(shaders.getCount());
  pipelineCI.renderPass = renderPass;
  pipelineCI.subpass = 0;

  auto rv = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline);
  assert(rv == VK_SUCCESS);
}

RenderBuffer VulkanContext::createBuffer(size_t initialSize, VkImageUsageFlags usageFlags)
{
  RenderBuffer buf;
  //VkBuffer& buffer, VkDeviceMemory& bufferMem, size_t& actualSize,

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.pNext = nullptr;
  bufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufferCI.size = initialSize;
  bufferCI.queueFamilyIndexCount = 0;
  bufferCI.pQueueFamilyIndices = nullptr;
  bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCI.flags = 0;
  auto rv = vkCreateBuffer(device, &bufferCI, nullptr, &buf.buffer);
  assert(rv == VK_SUCCESS);

  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, buf.buffer, &memReqs);
  buf.size = memReqs.size;

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = NULL;
  allocInfo.memoryTypeIndex = 0;
  allocInfo.allocationSize = memReqs.size;
  auto rvb = getMemoryTypeIndex(allocInfo.memoryTypeIndex, memReqs.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  assert(rvb);

  rv = vkAllocateMemory(device, &allocInfo, NULL, &buf.mem);
  assert(rv == VK_SUCCESS);

  rv = vkBindBufferMemory(device, buf.buffer, buf.mem, 0);
  assert(rv == VK_SUCCESS);

  return buf;
}

void VulkanContext::createImage(RenderImage& renderImage, uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format)
{
  renderImage.format = format;


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
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
  if (props.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    imageCI.tiling = VK_IMAGE_TILING_LINEAR;
  }
  else if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  }
  else {
    assert(false && "depth format unsupported");
  }
  auto rv = vkCreateImage(device, &imageCI, nullptr, &renderImage.image);
  assert(rv == VK_SUCCESS);

  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(device, renderImage.image, &memReqs);

  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.pNext = nullptr;
  memAllocInfo.allocationSize = 0;
  memAllocInfo.memoryTypeIndex = 0;
  memAllocInfo.allocationSize = memReqs.size;

  

  auto rvb = getMemoryTypeIndex(memAllocInfo.memoryTypeIndex, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  assert(rvb);

  rv = vkAllocateMemory(device, &memAllocInfo, NULL, &renderImage.mem);
  assert(rv == VK_SUCCESS);

  rv = vkBindImageMemory(device, renderImage.image, renderImage.mem, 0);
  assert(rv == VK_SUCCESS);

  VkImageViewCreateInfo imageViewCI = {};
  imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCI.pNext = NULL;
  imageViewCI.image = VK_NULL_HANDLE;
  imageViewCI.format = format;
  imageViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
  imageViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
  imageViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
  imageViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
  imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  imageViewCI.subresourceRange.baseMipLevel = 0;
  imageViewCI.subresourceRange.levelCount = 1;
  imageViewCI.subresourceRange.baseArrayLayer = 0;
  imageViewCI.subresourceRange.layerCount = 1;
  imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCI.flags = 0;
  imageViewCI.image = renderImage.image;
  rv = vkCreateImageView(device, &imageViewCI, NULL, &renderImage.view);
  assert(rv == VK_SUCCESS);
}

void VulkanContext::destroyImage(RenderImage renderImage)
{
  vkDestroyImageView(device, renderImage.view, nullptr);
  vkDestroyImage(device, renderImage.image, nullptr);
  vkFreeMemory(device, renderImage.mem, nullptr);
}


void VulkanContext::createFrameBuffers(Buffer<VkFramebuffer>& frameBuffers, VkRenderPass renderPass, VkImageView depthView, VkImageView* colorViews, uint32_t colorViewCount, uint32_t w, uint32_t h)
{
  VkImageView attachments[2];
  attachments[1] = depthView;

  VkFramebufferCreateInfo framebufferCI = {};
  framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCI.pNext = NULL;
  framebufferCI.renderPass = renderPass;
  framebufferCI.attachmentCount = 2;
  framebufferCI.pAttachments = attachments;
  framebufferCI.width = w;
  framebufferCI.height = h;
  framebufferCI.layers = 1;

  frameBuffers.accommodate(colorViewCount);
  for (uint32_t i = 0; i < colorViewCount; i++) {
    attachments[0] = colorViews[i];
    auto rv = vkCreateFramebuffer(device, &framebufferCI, NULL, frameBuffers.data()+i);
    assert(rv == VK_SUCCESS);
  }
}

void VulkanContext::destroyFrameBuffers(Buffer<VkFramebuffer>& frameBuffers)
{
  for (size_t i = 0; i < frameBuffers.getCount(); i++) {
    vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
  }
}

bool VulkanContext::getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements)
{
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    auto bit = 1 << i;
    if (typeBits & bit) {
      if ((memoryProperties.memoryTypes[i].propertyFlags & requirements) == requirements) {
        index = i;
        return true;
      }
    }
  }
  return false;
}