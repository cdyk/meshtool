#include <cassert>
#include "VulkanContext.h"

namespace
{

  VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(VkDebugReportFlagsEXT       flags,
                                                       VkDebugReportObjectTypeEXT  objectType,
                                                       uint64_t                    object,
                                                       size_t                      location,
                                                       int32_t                     messageCode,
                                                       const char*                 pLayerPrefix,
                                                       const char*                 pMessage,
                                                       void*                       pUserData)
  {
    auto logger = (Logger)pUserData;
    logger(0, "Vulkan@%s: %s", pLayerPrefix, pMessage);

    //std::cerr << pMessage << std::endl;
    return VK_FALSE;
  }
}


VulkanContext::VulkanContext(Logger logger,
                             const char** instanceExts, uint32_t instanceExtCount,
                             int hasPresentationSupport(VkInstance, VkPhysicalDevice, uint32_t queueFamily)) :
  logger(logger),
  physicalDevice(physicalDevice),
  device(device)
{
  // create instance
  { 
    const char* enabledLayers[] = { "VK_LAYER_LUNARG_standard_validation" };
    auto enabledLayerCount = uint32_t(sizeof(enabledLayers) / sizeof(const char*));

    const char* extraInstanceExts[] = { "VK_EXT_debug_report" };
    uint32_t extraInstanceExtCont = uint32_t(sizeof(extraInstanceExts) / sizeof(const char*));

    Buffer<const char*> allInstanceExts;
    allInstanceExts.accommodate(instanceExtCount + extraInstanceExtCont);
    for (uint32_t i = 0; i < instanceExtCount; i++) allInstanceExts[i] = instanceExts[i];
    for (uint32_t i = 0; i < extraInstanceExtCont; i++) allInstanceExts[instanceExtCount + i] = extraInstanceExts[i];

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
    instanceCI.enabledExtensionCount = instanceExtCount + extraInstanceExtCont;
    instanceCI.ppEnabledExtensionNames = allInstanceExts.data();
    instanceCI.enabledLayerCount = enabledLayerCount;
    instanceCI.ppEnabledLayerNames = enabledLayers;

    auto rv = vkCreateInstance(&instanceCI, nullptr, &instance);
    assert(rv == 0 && "vkCreateInstance");


    VkDebugReportCallbackCreateInfoEXT debugCallbackCI;
    debugCallbackCI.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugCallbackCI.pNext = nullptr;
    debugCallbackCI.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debugCallbackCI.pfnCallback = &MyDebugReportCallback;
    debugCallbackCI.pUserData = logger;

    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    VkResult result = vkCreateDebugReportCallbackEXT(instance, &debugCallbackCI, nullptr, &debugCallback);

    //auto vkDebugReportMessageEXT = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT");
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

    const char* deviceExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

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

  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
}

VulkanContext::~VulkanContext()
{
  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuf);
  vkDestroyCommandPool(device, cmdPool, nullptr);
  vkDestroyDevice(device, nullptr);
  if (debugCallback) {
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);
  }
  vkDestroyInstance(instance, nullptr);
}

void VulkanContext::houseKeep()
{
  {
    uint32_t d = 0;
    renderPassResources.purge();
    for (auto * r = renderPassResources.getPurged(); r; r = renderPassResources.getPurged()) {
      delete r; d++;
    }
    if (d) logger(0, "Deleted %d renderpasses", d);
  }
  {
    uint32_t d = 0;
    frameBufferResources.purge();
    for (auto * r = frameBufferResources.getPurged(); r; r = frameBufferResources.getPurged()) {
      delete r; d++;
    }
    if (d) logger(0, "Deleted %d framebuffers", d);
  }
  {
    uint32_t d = 0;
    renderImageResources.purge();
    for (auto * r = renderImageResources.getPurged(); r; r = renderImageResources.getPurged()) {
      destroyRenderImage(r);
      delete r; d++;
    }
    if (d) logger(0, "Deleted %d render images", d);
  }
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

//void VulkanContext::createImage(RenderImage& renderImage, uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format)

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
  if(renderImage->view && !renderImage->hasFlag(RenderImage::Flags::External)) vkDestroyImageView(device, renderImage->view, nullptr);
  renderImage->view = VK_NULL_HANDLE;

  if(renderImage->image && !renderImage->hasFlag(RenderImage::Flags::External))vkDestroyImage(device, renderImage->image, nullptr);
  renderImage->image = VK_NULL_HANDLE;

  if(renderImage->mem && !renderImage->hasFlag(RenderImage::Flags::External)) vkFreeMemory(device, renderImage->mem, nullptr);
  renderImage->mem = VK_NULL_HANDLE;
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


void VulkanContext::destroyRenderPass(RenderPass* pass)
{
  if (pass->pass) vkDestroyRenderPass(device, pass->pass, nullptr);
  pass->pass = nullptr;
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