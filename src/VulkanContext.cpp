#include <cassert>
#include "VulkanContext.h"

namespace
{
  VKAPI_ATTR VkBool32 VKAPI_CALL myDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                 void* pUserData)
  {
    auto logger = (Logger)pUserData;
    logger(0, "Vulkans: %s", pCallbackData->pMessage);
    return VK_FALSE;
  }


}


VulkanContext::VulkanContext(Logger logger,
                             const char** instanceExts, uint32_t instanceExtCount,
                             int hasPresentationSupport(VkInstance, VkPhysicalDevice, uint32_t queueFamily)) :
  logger(logger)
{
  // create instance
  { 
    Vector<const char*> layers;
    if (debugLayer) {
      layers.pushBack("VK_LAYER_LUNARG_standard_validation");
    }
    {
      uint32_t N;
      vkEnumerateInstanceLayerProperties(&N, nullptr);
      Vector<VkLayerProperties> layersPresent(N);
      vkEnumerateInstanceLayerProperties(&N, layersPresent.data());
      for (auto * layerName : layers) {
        bool found = false;
        for (auto & layer : layersPresent) {
          if (strcmp(layerName, layer.layerName) == 0) {
            found = true;
            break;
          }
        }
        if (!found) {
          logger(2, "Vulkan instance layer %s not supported", layerName);
          assert(false);
        }
        logger(0, "Vulkan instance layer %s is supported", layerName);
      }
    }

    Vector<const char*> instanceExtensions;
    for (uint32_t i = 0; i < instanceExtCount; i++) {
      instanceExtensions.pushBack(instanceExts[i]);
    }
    if (debugLayer) {
      instanceExtensions.pushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = nullptr;
    app_info.applicationVersion = 1;
    app_info.pEngineName = nullptr;
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceCI = {};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext = nullptr;
    instanceCI.flags = 0;
    instanceCI.pApplicationInfo = &app_info;
    instanceCI.enabledExtensionCount = instanceExtensions.size32();
    instanceCI.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCI.enabledLayerCount = layers.size32();
    instanceCI.ppEnabledLayerNames = layers.data();
    auto rv = vkCreateInstance(&instanceCI, nullptr, &instance);
    assert(rv == 0 && "vkCreateInstance");

    if (debugLayer) {
      VkDebugUtilsMessengerCreateInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      info.pfnUserCallback = myDebugCallback;
      info.pUserData = logger;

      auto createDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
      assert(createDebugUtilsMessenger);

      auto rv = createDebugUtilsMessenger(instance, &info, nullptr, &debugCallbackHandle);
      assert(rv == VK_SUCCESS);
    }
  }
  // choose physical device
  {
    Buffer<VkPhysicalDevice> devices;

    uint32_t gpuCount = 0;
    auto rv = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    devices.accommodate(gpuCount);
    rv = vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data());
    assert(rv == 0 && "vkEnumeratePhysicalDevices");
    assert(gpuCount);

    size_t chosenDevice = 0;
    VkPhysicalDeviceProperties chosenProps = { 0 };
    vkGetPhysicalDeviceProperties(devices[0], &chosenProps);
    for (size_t i = 0; i < gpuCount; i++) {
      VkPhysicalDeviceProperties props = { 0 };
      vkGetPhysicalDeviceProperties(devices[i], &props);

      uint64_t vmem = 0;
      VkPhysicalDeviceMemoryProperties memProps = { 0 };
      vkGetPhysicalDeviceMemoryProperties(devices[i], &memProps);
      for (uint32_t k = 0; k < memProps.memoryHeapCount; k++) {
        if (memProps.memoryHeaps[k].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
          vmem = memProps.memoryHeaps[k].size;
        }
      }
      logger(0, "Device %d: name='%s', type=%d, mem=%lld", i, props.deviceName, props.deviceType, vmem);
    }
    physicalDevice = devices[chosenDevice];
  }
  // create device and queue
  { 
    VkDeviceQueueCreateInfo queueInfo = {};

    uint32_t queueFamilyCount = 0;
    Buffer<VkQueueFamilyProperties> queueProps;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    queueProps.accommodate(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps.data());

    bool found = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        if (hasPresentationSupport(instance, physicalDevice, i)) {
          queueInfo.queueFamilyIndex = i;
          found = true;
          break;
        }
      }
    }
    assert(found);
    queueFamilyIndex = queueInfo.queueFamilyIndex;

    float queuePriorities[1] = { 0.0 };
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.pNext = nullptr;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queuePriorities;

    const char* deviceExt[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      //VK_EXT_DEBUG_MARKER_EXTENSION_NAME
    };
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = uint32_t(sizeof(deviceExt) / sizeof(const char*));
    deviceInfo.ppEnabledExtensionNames = deviceExt;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.pEnabledFeatures = nullptr;

    auto rv = vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device);
    assert(rv == VK_SUCCESS);

    //vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)vkGetInstanceProcAddr(instance, "vkCmdDebugMarkerBeginEXT");
    //vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)vkGetInstanceProcAddr(instance, "vkCmdDebugMarkerEndEXT");
    //vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(instance, "vkDebugMarkerSetObjectNameEXT");

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
  }
  // create descriptoor pool
  {
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * uint32_t(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    pool_info.poolSizeCount = uint32_t(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    pool_info.pPoolSizes = poolSizes;
    auto rv = vkCreateDescriptorPool(device, &pool_info, nullptr, &descPool);
    assert(rv == VK_SUCCESS);
  }
#if 0
  // create command pool
  {
    VkCommandPoolCreateInfo cmdPoolCI = {};
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.pNext = nullptr;
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
    cmdPoolCI.flags = 0;
    auto rv = vkCreateCommandPool(device, &cmdPoolCI, NULL, &cmdPool);
    assert(rv == VK_SUCCESS);
  }
  // create command buffer
  {
    VkCommandBufferAllocateInfo cmdBufAllocCI = {};
    cmdBufAllocCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocCI.pNext = NULL;
    cmdBufAllocCI.commandPool = cmdPool;
    cmdBufAllocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocCI.commandBufferCount = 1;
    auto rv = vkAllocateCommandBuffers(device, &cmdBufAllocCI, &cmdBuf);
    assert(rv == VK_SUCCESS);
  }
#endif
  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

VulkanContext::~VulkanContext()
{
  houseKeep();
  logger(0, "%d unreleased fences", fenceResources.getCount());
  logger(0, "%d unreleased buffers", bufferResources.getCount());
  logger(0, "%d unreleased descriptors", descriptorSetResources.getCount());
  logger(0, "%d unreleased shaders", shaderResources.getCount());
  logger(0, "%d unreleased pipelines", pipelineResources.getCount());
  logger(0, "%d unreleased renderPasses", renderPassResources.getCount());
  logger(0, "%d unreleased frameBuffers", frameBufferResources.getCount());
  logger(0, "%d unreleased renderImages", renderImageResources.getCount());


  //vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuf);
  //vkDestroyCommandPool(device, cmdPool, nullptr);
  vkDestroyDevice(device, nullptr);

  if (debugLayer) {
    auto destroyDebugMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    assert(destroyDebugMessenger);
    destroyDebugMessenger(instance, debugCallbackHandle, nullptr);
  }
  vkDestroyInstance(instance, nullptr);
}

void VulkanContext::houseKeep()
{
  {
    Vector<RenderFence*> orphans;
    fenceResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyFence(r);
      delete r;
    }
  }

  {
    Vector<RenderBuffer*> orphans;
    bufferResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyBuffer(r);
      delete r;
    }
  }

  {
    Vector<DescriptorSet*> orphans;
    descriptorSetResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyDescriptorSet(r);
      delete r;
    }
  }

  {
    Vector<Shader*> orphans;
    shaderResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyShader(r);
      delete r;
    }
  }

  {
    Vector<Pipeline*> orphans;
    pipelineResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyPipeline(r);
      delete r;
    }
  }

  {
    Vector<RenderPass*> orphans;
    renderPassResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyRenderPass(r);
      delete r;
    }
  }

  {
    Vector<FrameBuffer*> orphans;
    frameBufferResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyFrameBuffer(r);
      delete r;
    }
  }

  {
    Vector<RenderImage*> orphans;
    renderImageResources.getOrphans(orphans);
    for (auto * r : orphans) {
      if (!r->hasFlag(ResourceBase::Flags::External)) destroyRenderImage(r);
      delete r;
    }
  }

}

void VulkanContext::annotate(VkDebugReportObjectTypeEXT type, uint64_t object, const char* name)
{
  //VkDebugMarkerObjectNameInfoEXT info{};
  //info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
  //info.objectType = type;
  //info.object = object;
  //info.pObjectName = name;
  //vkDebugMarkerSetObjectNameEXT(device, &info);
}



PipelineHandle VulkanContext::createPipeline(Vector<VkVertexInputBindingDescription>& inputBind,
                                             Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                             VkPipelineLayoutCreateInfo& pipelineLayoutInfo,
                                             VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                             RenderPassHandle renderPass,
                                             ShaderHandle shader,
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

  auto rv = vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &pipe->descLayout);
  assert(rv == VK_SUCCESS);

  pipelineLayoutInfo.setLayoutCount = 1;// uint32_t(descSetLayout.getCount());
  pipelineLayoutInfo.pSetLayouts = &pipe->descLayout;
  rv = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipe->pipeLayout);
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
  pipelineCI.pStages = shader.resource->stageCreateInfo.data();
  pipelineCI.stageCount = uint32_t(shader.resource->stageCreateInfo.size());
  pipelineCI.renderPass = renderPass.resource->pass;
  pipelineCI.subpass = 0;

  rv = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipe->pipe);
  assert(rv == VK_SUCCESS);

  logger(0, "Built pipeline");
  return pipeHandle;
}

void VulkanContext::destroyPipeline(Pipeline* pipe)
{
  if (pipe->pipe) vkDestroyPipeline(device, pipe->pipe, nullptr);
  if (pipe->pipeLayout) vkDestroyPipelineLayout(device, pipe->pipeLayout, nullptr);
  if (pipe->descLayout) vkDestroyDescriptorSetLayout(device, pipe->descLayout, nullptr);
}


RenderBufferHandle VulkanContext::createBuffer(size_t requestedSize, VkImageUsageFlags usageFlags)
{
  auto bufHandle = bufferResources.createResource();
  auto * buf = bufHandle.resource;
  buf->requestedSize = requestedSize;

  //VkBuffer& buffer, VkDeviceMemory& bufferMem, size_t& actualSize,

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.pNext = nullptr;
  bufferCI.usage = usageFlags;
  bufferCI.size = requestedSize;
  bufferCI.queueFamilyIndexCount = 0;
  bufferCI.pQueueFamilyIndices = nullptr;
  bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  bufferCI.flags = 0;
  auto rv = vkCreateBuffer(device, &bufferCI, nullptr, &buf->buffer);
  assert(rv == VK_SUCCESS);

  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, buf->buffer, &memReqs);
  buf->alignedSize = memReqs.size;

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.pNext = NULL;
  allocInfo.memoryTypeIndex = 0;
  allocInfo.allocationSize = memReqs.size;
  auto rvb = getMemoryTypeIndex(allocInfo.memoryTypeIndex, memReqs.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  assert(rvb);

  rv = vkAllocateMemory(device, &allocInfo, NULL, &buf->mem);
  assert(rv == VK_SUCCESS);

  rv = vkBindBufferMemory(device, buf->buffer, buf->mem, 0);
  assert(rv == VK_SUCCESS);

  return bufHandle;
}


void VulkanContext::destroyBuffer(RenderBuffer* buffer)
{
  if (buffer->buffer) vkDestroyBuffer(device, buffer->buffer, nullptr);
  if (buffer->mem) vkFreeMemory(device, buffer->mem, nullptr);
}


DescriptorSetHandle VulkanContext::createDescriptorSet(VkDescriptorSetLayout descLayout)
{
  auto descSetHandle = descriptorSetResources.createResource();
  auto * descSet = descSetHandle.resource;

  VkDescriptorSetAllocateInfo alloc_info[1];
  alloc_info[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info[0].pNext = nullptr;
  alloc_info[0].descriptorPool = descPool;
  alloc_info[0].descriptorSetCount = 1;
  alloc_info[0].pSetLayouts = &descLayout;

  auto rv = vkAllocateDescriptorSets(device, alloc_info, &descSet->descSet);
  assert(rv == VK_SUCCESS);

  return descSetHandle;
}

void VulkanContext::destroyDescriptorSet(DescriptorSet* descSet)
{
  if (descSet->descSet) vkFreeDescriptorSets(device, descPool, 1, &descSet->descSet);
}

void VulkanContext::updateDescriptorSet(DescriptorSetHandle descriptorSet, RenderBufferHandle buffer)
{
  VkDescriptorBufferInfo descBufInfo;
  descBufInfo.buffer = buffer.resource->buffer;
  descBufInfo.offset = 0;
  descBufInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet writes[1];
  writes[0] = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = nullptr;
  writes[0].dstSet = descriptorSet.resource->descSet;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &descBufInfo;
  writes[0].dstArrayElement = 0;
  writes[0].dstBinding = 0;
  vkUpdateDescriptorSets(device, 1, writes, 0, NULL);
}


ShaderHandle VulkanContext::createShader(Vector<ShaderInputSpec>& spec, const char* name)
{
  char buf[256];

  auto shaderHandle = shaderResources.createResource();
  auto * shader = shaderHandle.resource;

  shader->stageCreateInfo.resize(spec.size());
  for (size_t i = 0; i < spec.size(); i++) {
    shader->stageCreateInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader->stageCreateInfo[i].pNext = nullptr;
    shader->stageCreateInfo[i].pSpecializationInfo = nullptr;
    shader->stageCreateInfo[i].flags = 0;
    shader->stageCreateInfo[i].pName = "main";
    shader->stageCreateInfo[i].stage = spec[i].stage;

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = nullptr;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = spec[i].siz;
    moduleCreateInfo.pCode = spec[i].spv;
    auto rv = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shader->stageCreateInfo[i].module);
    assert(rv == VK_SUCCESS);

    if (name) {
      snprintf(buf, sizeof(buf), "%s:stage%d", name, int(i));
      annotate(VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, uint64_t(shader->stageCreateInfo[i].module), buf);
    }

  }
  return shaderHandle;
}

void VulkanContext::destroyShader(Shader* shader)
{
  for (size_t i = 0; i < shader->stageCreateInfo.size(); i++) {
    vkDestroyShaderModule(device, shader->stageCreateInfo[i].module, nullptr);
  }
  shader->stageCreateInfo.resize(0);
}


RenderImageHandle VulkanContext::wrapRenderImageView(VkImageView view)
{
  auto renderImageHandle = renderImageResources.createResource();
  auto * renderImage = renderImageHandle.resource;
  renderImage->setFlag(ResourceBase::Flags::External);
  renderImage->view = view;
  return renderImageHandle;
}


RenderImageHandle VulkanContext::createRenderImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format)
{
  auto renderImageHandle = renderImageResources.createResource();
  auto * renderImage = renderImageHandle.resource;

  renderImage->format = format;

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
  auto rv = vkCreateImage(device, &imageCI, nullptr, &renderImage->image);
  assert(rv == VK_SUCCESS);

  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(device, renderImage->image, &memReqs);

  VkMemoryAllocateInfo memAllocInfo = {};
  memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAllocInfo.pNext = nullptr;
  memAllocInfo.allocationSize = 0;
  memAllocInfo.memoryTypeIndex = 0;
  memAllocInfo.allocationSize = memReqs.size;

  auto rvb = getMemoryTypeIndex(memAllocInfo.memoryTypeIndex, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  assert(rvb);

  rv = vkAllocateMemory(device, &memAllocInfo, NULL, &renderImage->mem);
  assert(rv == VK_SUCCESS);

  rv = vkBindImageMemory(device, renderImage->image, renderImage->mem, 0);
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
  imageViewCI.image = renderImage->image;
  rv = vkCreateImageView(device, &imageViewCI, NULL, &renderImage->view);
  assert(rv == VK_SUCCESS);

  return renderImageHandle;
}


void VulkanContext::destroyRenderImage(RenderImage* renderImage)
{
  if(renderImage->view) vkDestroyImageView(device, renderImage->view, nullptr);
  if(renderImage->image)vkDestroyImage(device, renderImage->image, nullptr);
  if(renderImage->mem) vkFreeMemory(device, renderImage->mem, nullptr);
}


RenderPassHandle VulkanContext::createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                                 VkSubpassDescription* subpasses, uint32_t subpassCount)
{
  auto passHandle = renderPassResources.createResource();
  auto * pass = passHandle.resource;

  {
    VkRenderPassCreateInfo rp_info = {};

    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = subpassCount;
    rp_info.pSubpasses = subpasses;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;

    auto rv = vkCreateRenderPass(device, &rp_info, NULL, &pass->pass);
    assert(rv == VK_SUCCESS);
  }

  return passHandle;
}


RenderFenceHandle VulkanContext::createFence(bool signalled)
{
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.flags = signalled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

  auto fenceHandle = fenceResources.createResource();
  auto rv = vkCreateFence(device, &info, nullptr, &fenceHandle.resource->fence);
  assert(rv == VK_SUCCESS);
  return fenceHandle;
}

void VulkanContext::destroyRenderPass(RenderPass* pass)
{
  if (pass->pass) vkDestroyRenderPass(device, pass->pass, nullptr);
  pass->pass = nullptr;
}


void VulkanContext::destroyFence(RenderFence* fence)
{
  if (fence->fence) vkDestroyFence(device, fence->fence, nullptr);
  fence->fence = nullptr;
}


FrameBufferHandle VulkanContext::createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<RenderImageHandle>& attachments)
{
  auto fbHandle = frameBufferResources.createResource();
  auto * fb = fbHandle.resource;

  fb->pass = pass;

  Buffer<VkImageView> att(attachments.size());
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
  auto rv = vkCreateFramebuffer(device, &info, NULL, &fb->fb);
  assert(rv == VK_SUCCESS);

  return fbHandle;
}


void VulkanContext::destroyFrameBuffer(FrameBuffer* fb)
{
  if (fb->fb) vkDestroyFramebuffer(device, fb->fb, nullptr);
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

MappedBufferBase::MappedBufferBase(void** ptr, VulkanContext* vCtx, RenderBufferHandle h)
  : vCtx(vCtx), h(h)
{
  auto rv = vkMapMemory(vCtx->device, h.resource->mem, 0, h.resource->alignedSize, 0, ptr);
  assert(rv == VK_SUCCESS);

}

MappedBufferBase::~MappedBufferBase()
{
  vkUnmapMemory(vCtx->device, h.resource->mem);
}

DebugScope::DebugScope(VulkanContext* vCtx, VkCommandBuffer cmdBuf, const char* name) :
  vCtx(vCtx),
  cmdBuf(cmdBuf)
{
  // dependes on VK_EXT_debug_report
  //VkDebugMarkerMarkerInfoEXT info = {};
  //info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
  //info.pMarkerName = name;
  //vCtx->vkCmdDebugMarkerBeginEXT(cmdBuf, &info);
}

DebugScope::~DebugScope()
{
  //vCtx->vkCmdDebugMarkerEndEXT(cmdBuf);
}
