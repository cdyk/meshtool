#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cassert>
#include "VulkanContext.h"
#include "VulkanInfos.h"
#include "VulkanResources.h"
#include "VulkanFrameManager.h"


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
                             uint32_t framesInFlight, ISurfaceManager* presentationSupport) :
  logger(logger),
  surfaceManager(presentationSupport)
{

  // create instance
  { 
    Vector<const char*> layers;
    if (debugLayer) {
      layers.pushBack("VK_LAYER_LUNARG_standard_validation");
      layers.pushBack("VK_LAYER_LUNARG_assistant_layer");
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
    Buffer<VkExtensionProperties> extensionProps;
    Buffer<VkQueueFamilyProperties> queueProps;
    for (size_t i = 0; i < gpuCount; i++) {
      physicalDevice = devices[i];

      VkPhysicalDeviceProperties props = { 0 };
      vkGetPhysicalDeviceProperties(devices[i], &props);

      VkPhysicalDeviceFeatures features{};
      vkGetPhysicalDeviceFeatures(devices[i], &features);

      uint64_t vmem = 0;
      VkPhysicalDeviceMemoryProperties memProps = { 0 };
      vkGetPhysicalDeviceMemoryProperties(devices[i], &memProps);
      for (uint32_t k = 0; k < memProps.memoryHeapCount; k++) {
        if (memProps.memoryHeaps[k].flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
          vmem = memProps.memoryHeaps[k].size;
        }
      }
      logger(0, "Device %d: name='%s', type=%d, mem=%lld, fillModeNonSolid=%d", i, props.deviceName, props.deviceType, vmem, features.fillModeNonSolid);

      uint32_t extensionCount = 0;
      vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, nullptr);
      extensionProps.accommodate(extensionCount);
      vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &extensionCount, extensionProps.data());
      for (uint32_t k = 0; k < extensionCount; k++) {
        auto &e = extensionProps[k];
        logger(0, "Device %d: extension \"%s\", spec version=%d",i, e.extensionName, e.specVersion);

        if (std::strcmp(e.extensionName, VK_NVX_RAYTRACING_EXTENSION_NAME) == 0) {
          nvxRaytracing = true;
        }
      }


      for (uint32_t k = 0; k < memProps.memoryHeapCount; k++) {
        auto & h = memProps.memoryHeaps[k];
        logger(0, "Device %d: Memory heap %d: size=%5dMB flags=%s%s%s", i, k, h.size / (1024 * 1024),
               h.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "DEVICE_LOCAL " : "",
               h.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT ? "MULTI_INSTANCE " : "",
               h.flags ? "" : "NONE");
      }
      for (uint32_t k = 0; k < memProps.memoryTypeCount; k++) {
        auto & t = memProps.memoryTypes[k];
        logger(0, "Device %d: Memory type %2d: heap=%d, flags=%s%s%s%s%s%s%s%s", i, k, t.heapIndex,
               t.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ? "DEVICE_LOCAL " : "",
               t.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? "HOST_VISIBLE " : "",
               t.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ? "HOST_COHERENT " : "",
               t.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT ? "HOST_CACHED " : "",
               t.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ? "LAZILY_ALLOCATED " : "",
               t.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT ? "PROTECTED " : "",
               t.propertyFlags & ~(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT) ? "??? " : "",
               t.propertyFlags ? "" : "NONE");
      }
      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, nullptr);
      queueProps.accommodate(queueFamilyCount);

      vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, queueProps.data());
      for (uint32_t k = 0; k < queueFamilyCount; k++) {
        auto & qp = queueProps[k];
        logger(0, "Device %d: Queue family %d: count=%2d, granularity=%dx%dx%d, flags=%s%s%s%s%s%s%s", i, k, qp.queueCount,
               qp.minImageTransferGranularity.width, qp.minImageTransferGranularity.height, qp.minImageTransferGranularity.depth,
               presentationSupport->hasPresentationSupport(this, k) ? "PRESENTATION " : "",
               qp.queueFlags & VK_QUEUE_GRAPHICS_BIT ? "GRAPHICS " : "",
               qp.queueFlags & VK_QUEUE_COMPUTE_BIT ? "COMPUTE " : "",
               qp.queueFlags & VK_QUEUE_TRANSFER_BIT ? "TRANSFER " : "",
               qp.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ? "SPARSE_BINDING " : "",
               qp.queueFlags & VK_QUEUE_PROTECTED_BIT ? "PROTECTED " : "",
               qp.queueFlags ? "" : "NONE");
      }
      logger(0, "----------");
    }
    physicalDevice = devices[chosenDevice];
    logger(0, "Chose device %d.", chosenDevice);

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  }
  // create device and queue
  { 

    uint32_t queueFamilyCount = 0;
    Buffer<VkQueueFamilyProperties> queueProps;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    queueProps.accommodate(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps.data());

    


    VkDeviceQueueCreateInfo queueInfo = {};

    bool found = false;
    for (uint32_t queueFamilyIx = 0; queueFamilyIx < queueFamilyCount; queueFamilyIx++) {
      if (queueProps[queueFamilyIx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        if(presentationSupport->hasPresentationSupport(this, queueFamilyIx)) {
          queueInfo.queueFamilyIndex = queueFamilyIx;
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

    Vector<const char*> requestedDeviceExtensions;
    requestedDeviceExtensions.pushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (nvxRaytracing) {
      requestedDeviceExtensions.pushBack(VK_NVX_RAYTRACING_EXTENSION_NAME);
      requestedDeviceExtensions.pushBack(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    }

    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT };
    VkPhysicalDeviceFeatures2 features2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.fillModeNonSolid = 1;
    enabledFeatures.samplerAnisotropy = 1;
    if (nvxRaytracing) {
      enabledFeatures.vertexPipelineStoresAndAtomics = 1;
      features2.pNext = &descriptorIndexing;
    }

    // I guess this basically enables anything the device has to offer...
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    deviceInfo.pNext = &features2;  // FIXME: Non-null ptr not checked on non-RTX.
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = requestedDeviceExtensions.size32();
    deviceInfo.ppEnabledExtensionNames = requestedDeviceExtensions.data();
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.pEnabledFeatures = &enabledFeatures;

    CHECK_VULKAN(vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device));

    if (nvxRaytracing) {
      vkCreateAccelerationStructureNVX = (PFN_vkCreateAccelerationStructureNVX)vkGetInstanceProcAddr(instance, "vkCreateAccelerationStructureNVX");;
      assert(vkCreateAccelerationStructureNVX);
      vkDestroyAccelerationStructureNVX = (PFN_vkDestroyAccelerationStructureNVX)vkGetInstanceProcAddr(instance, "vkDestroyAccelerationStructureNVX");;
      assert(vkDestroyAccelerationStructureNVX);
      vkGetAccelerationStructureMemoryRequirementsNVX = (PFN_vkGetAccelerationStructureMemoryRequirementsNVX)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureMemoryRequirementsNVX");;
      assert(vkGetAccelerationStructureMemoryRequirementsNVX);
      vkGetAccelerationStructureScratchMemoryRequirementsNVX = (PFN_vkGetAccelerationStructureScratchMemoryRequirementsNVX)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureScratchMemoryRequirementsNVX");;
      assert(vkGetAccelerationStructureScratchMemoryRequirementsNVX);
      vkBindAccelerationStructureMemoryNVX = (PFN_vkBindAccelerationStructureMemoryNVX)vkGetInstanceProcAddr(instance, "vkBindAccelerationStructureMemoryNVX");;
      assert(vkBindAccelerationStructureMemoryNVX);
      vkCmdBuildAccelerationStructureNVX = (PFN_vkCmdBuildAccelerationStructureNVX)vkGetInstanceProcAddr(instance, "vkCmdBuildAccelerationStructureNVX");;
      assert(vkCmdBuildAccelerationStructureNVX);
      vkCmdCopyAccelerationStructureNVX = (PFN_vkCmdCopyAccelerationStructureNVX)vkGetInstanceProcAddr(instance, "vkCmdCopyAccelerationStructureNVX");;
      assert(vkCmdCopyAccelerationStructureNVX);
      vkCmdTraceRaysNVX = (PFN_vkCmdTraceRaysNVX)vkGetInstanceProcAddr(instance, "vkCmdTraceRaysNVX");;
      assert(vkCmdTraceRaysNVX);
      vkCreateRaytracingPipelinesNVX = (PFN_vkCreateRaytracingPipelinesNVX)vkGetInstanceProcAddr(instance, "vkCreateRaytracingPipelinesNVX");;
      assert(vkCreateRaytracingPipelinesNVX);
      vkGetRaytracingShaderHandlesNVX = (PFN_vkGetRaytracingShaderHandlesNVX)vkGetInstanceProcAddr(instance, "vkGetRaytracingShaderHandlesNVX");;
      assert(vkGetRaytracingShaderHandlesNVX);
      vkGetAccelerationStructureHandleNVX = (PFN_vkGetAccelerationStructureHandleNVX)vkGetInstanceProcAddr(instance, "vkGetAccelerationStructureHandleNVX");;
      assert(vkGetAccelerationStructureHandleNVX);
      vkCmdWriteAccelerationStructurePropertiesNVX = (PFN_vkCmdWriteAccelerationStructurePropertiesNVX)vkGetInstanceProcAddr(instance, "vkCmdWriteAccelerationStructurePropertiesNVX");;
      assert(vkCmdWriteAccelerationStructurePropertiesNVX);
      vkCompileDeferredNVX = (PFN_vkCompileDeferredNVX)vkGetInstanceProcAddr(instance, "vkCompileDeferredNVX");;
      assert(vkCompileDeferredNVX);
    }
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
    VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * uint32_t(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    pool_info.poolSizeCount = uint32_t(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    pool_info.pPoolSizes = poolSizes;
    CHECK_VULKAN(vkCreateDescriptorPool(device, &pool_info, nullptr, &descPool));
  }

  {
    VkPipelineCacheCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    CHECK_VULKAN(vkCreatePipelineCache(device, &info, nullptr, &pipelineCache));
  }

  infos = new VulkanInfos();
  resources = new VulkanResources(this, logger);
  frameManager = new VulkanFrameManager(this, logger, framesInFlight);
}


void VulkanContext::init()
{
  resources->init();
  frameManager->init();
}


void VulkanContext::houseKeep()
{
  resources->houseKeep();
  frameManager->houseKeep();
}

void VulkanContext::shutdown()
{
  frameManager->houseKeep();
  delete frameManager;
  frameManager = nullptr;

  resources->houseKeep();
  delete resources;
  resources = nullptr;

  delete infos; infos = nullptr;
  vkDestroyDevice(device, nullptr);

  if (debugLayer) {
    auto destroyDebugMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    assert(destroyDebugMessenger);
    destroyDebugMessenger(instance, debugCallbackHandle, nullptr);
  }
  vkDestroyInstance(instance, nullptr);
}


VulkanContext::~VulkanContext()
{

  
}
