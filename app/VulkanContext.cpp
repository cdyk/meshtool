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
    for (uint32_t queueFamilyIx = 0; queueFamilyIx < queueFamilyCount; queueFamilyIx++) {
      if (queueProps[queueFamilyIx].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        if(presentationSupport->hasPresentationSupport(this, queueFamilyIx)) {
        //if(glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueFamilyIx)) {
        //if (hasPresentationSupport(queueFamilyIx)) {
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

    const char* deviceExt[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      //VK_EXT_DEBUG_MARKER_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.fillModeNonSolid = 1;
    enabledFeatures.samplerAnisotropy = 1;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = uint32_t(sizeof(deviceExt) / sizeof(const char*));
    deviceInfo.ppEnabledExtensionNames = deviceExt;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.pEnabledFeatures = &enabledFeatures;

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
  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);


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

  CHECK_VULKAN(vkDeviceWaitIdle(device));

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
