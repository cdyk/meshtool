#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
//#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <list>
#include <vector>
#include <cctype>
#include <chrono>
#include <mutex>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>

#include <cstdio>
#include "Viewer.h"
#include "Common.h"
#include "Mesh.h"
#include "LinAlgOps.h"
#include "Renderer.h"
#include "VulkanContext.h"

namespace {

  Viewer viewer;
  Tasks tasks;
  int width, height;
  float leftSplit = 100;
  float thickness = 8;

  bool solid = true;

  VulkanContext* vCtx = nullptr;
  Renderer* renderer = nullptr;
  

  ImGui_ImplVulkanH_WindowData imguiWindowData;

  std::mutex incomingMeshLock;
  std::list<Mesh*> incomingMeshes;

  struct MeshItem
  {
    Mesh* mesh;
    RenderMesh* renderMesh;
  };

  std::list<MeshItem> meshItems;

  void logger(unsigned level, const char* msg, ...)
  {
    switch (level) {
    case 0: fprintf(stderr, "[I] "); break;
    case 1: fprintf(stderr, "[W] "); break;
    case 2: fprintf(stderr, "[E] "); break;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfprintf(stderr, msg, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
  }

  void glfw_error_callback(int error, const char* description)
  {
    logger(2, "GLFV error %d: %s", error, description);
  }
  
  void check_vk_result(VkResult err)
  {
    if (err == 0) return;
    logger(2, "VkResult %d", err);
    if (err < 0)
      abort();
  }

  bool createInstance(VkInstance& instance)
  {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = nullptr;
    app_info.applicationVersion = 1;
    app_info.pEngineName = nullptr;
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_1;

    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

    VkInstanceCreateInfo inst_info = {};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = extensions_count;
    inst_info.ppEnabledExtensionNames = extensions;
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;

    auto rv = vkCreateInstance(&inst_info, nullptr, &instance);
    assert(rv == 0 && "vkCreateInstance");
    
    return true;
  }

  bool choosePhysicalDevice(VkPhysicalDevice& physicalDevice, VkInstance instance)
  {
    uint32_t gpuCount = 0;
    auto rv = vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> devices(gpuCount);
    rv = vkEnumeratePhysicalDevices(instance, &gpuCount, devices.data());
    assert(rv == 0 && "vkEnumeratePhysicalDevices");
    assert(gpuCount);

    size_t chosenDevice = 0;
    VkPhysicalDeviceProperties chosenProps = { 0 };
    vkGetPhysicalDeviceProperties(devices[0], &chosenProps);
    for (size_t i = 0; i < devices.size(); i++) {
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
    return true;
  }

  bool createDevice(VkDevice& device, VkQueue& queue, uint32_t& queueFamilyIndex, VkInstance instance, VkPhysicalDevice physicalDevice)
  {
    VkDeviceQueueCreateInfo queueInfo = {};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount);
    std::vector<VkQueueFamilyProperties> queueProps(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueProps.data());

    bool found = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
      if (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        if (glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, i)) {
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
    deviceInfo.enabledExtensionCount = sizeof(deviceExt)/sizeof(const char*);
    deviceInfo.ppEnabledExtensionNames = deviceExt;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = nullptr;
    deviceInfo.pEnabledFeatures = nullptr;

    auto rv = vkCreateDevice(physicalDevice, &deviceInfo, NULL, &device);
    assert(rv == VK_SUCCESS);

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    return true;
  }

  bool createDescriptorPool(VkDescriptorPool& descPool, VkDevice device)
  {
    VkDescriptorPoolSize pool_sizes[] =
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
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    auto rv = vkCreateDescriptorPool(device, &pool_info, nullptr, &descPool);
    assert(rv == VK_SUCCESS);

    return true;
  }

  bool createCommandBuffer(VkCommandBuffer& cmdBuf, VkCommandPool& cmdPool, uint32_t queueFamilyIndex, VkDevice device)
  {
    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext = nullptr;
    cmd_pool_info.queueFamilyIndex = queueFamilyIndex;
    cmd_pool_info.flags = 0;

    auto rv = vkCreateCommandPool(device, &cmd_pool_info, NULL, &cmdPool);
    assert(rv == VK_SUCCESS);

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = cmdPool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    rv = vkAllocateCommandBuffers(device, &cmd, &cmdBuf);
    assert(rv == VK_SUCCESS);

    return true;
  }

  


  bool initSwapChain(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex)
  {
    VkSurfaceKHR surface;
    auto rv = glfwCreateWindowSurface(instance, window, nullptr, &surface);
    assert(rv == VK_SUCCESS);
    imguiWindowData.Surface = surface;

    VkBool32 supportsPresent = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &supportsPresent);
    assert(supportsPresent == VK_TRUE);

    uint32_t formatCount=0;
    rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL);
    assert(rv == VK_SUCCESS);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    rv = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    assert(rv == VK_SUCCESS);

    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    imguiWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDevice, surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    imguiWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(physicalDevice, surface, present_modes, IM_ARRAYSIZE(present_modes));
    return true;
  }

  void uploadFonts(VkDevice device, VkQueue queue)
  {
    VkCommandPool command_pool = imguiWindowData.Frames[imguiWindowData.FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = imguiWindowData.Frames[imguiWindowData.FrameIndex].CommandBuffer;

    auto rv = vkResetCommandPool(device, command_pool, 0);
    assert(rv == VK_SUCCESS);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    rv = vkBeginCommandBuffer(command_buffer, &begin_info);
    assert(rv == VK_SUCCESS);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    rv = vkEndCommandBuffer(command_buffer);
    assert(rv == VK_SUCCESS);

    rv = vkQueueSubmit(queue, 1, &end_info, VK_NULL_HANDLE);
    assert(rv == VK_SUCCESS);

    rv = vkDeviceWaitIdle(device);
    assert(rv == VK_SUCCESS);

    ImGui_ImplVulkan_InvalidateFontUploadObjects();
  }

  void frameRender(VkDevice device, VkQueue queue)
  {
    VkSemaphore& image_acquired_semaphore = imguiWindowData.Frames[imguiWindowData.FrameIndex].ImageAcquiredSemaphore;
    auto rv = vkAcquireNextImageKHR(device, imguiWindowData.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &imguiWindowData.FrameIndex);
    assert(rv == VK_SUCCESS);

    auto * frameData = &imguiWindowData.Frames[imguiWindowData.FrameIndex];
    rv = vkWaitForFences(device, 1, &frameData->Fence, VK_TRUE, UINT64_MAX);
    assert(rv == VK_SUCCESS);

    rv = vkResetFences(device, 1, &frameData->Fence);
    assert(rv == VK_SUCCESS);

    rv = vkResetCommandPool(device, frameData->CommandPool, 0);
    assert(rv == VK_SUCCESS);

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    rv = vkBeginCommandBuffer(frameData->CommandBuffer, &info);
    assert(rv == VK_SUCCESS);

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = imguiWindowData.RenderPass;
    beginInfo.framebuffer = imguiWindowData.Framebuffer[imguiWindowData.FrameIndex];
    beginInfo.renderArea.extent.width = imguiWindowData.Width;
    beginInfo.renderArea.extent.height = imguiWindowData.Height;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &imguiWindowData.ClearValue;

    vkCmdBeginRenderPass(frameData->CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameData->CommandBuffer);
    vkCmdEndRenderPass(frameData->CommandBuffer);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &image_acquired_semaphore;
    submitInfo.pWaitDstStageMask = &wait_stage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frameData->CommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameData->RenderCompleteSemaphore;

    rv = vkEndCommandBuffer(frameData->CommandBuffer);
    assert(rv == VK_SUCCESS);

    rv = vkQueueSubmit(queue, 1, &submitInfo, frameData->Fence);
    assert(rv == VK_SUCCESS);
  }

  void framePresent(VkQueue queue)
  {
    ImGui_ImplVulkanH_FrameData* fd = &imguiWindowData.Frames[imguiWindowData.FrameIndex];
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &fd->RenderCompleteSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &imguiWindowData.Swapchain;
    info.pImageIndices = &imguiWindowData.FrameIndex;
    auto rv = vkQueuePresentKHR(queue, &info);
    assert(rv == VK_SUCCESS);
  }

  void resizeFunc(GLFWwindow* window, int w, int h)
  {
  }

  void moveFunc(GLFWwindow* window, double x, double y)
  {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    viewer.move(float(x), float(y));
  }

  void buttonFunc(GLFWwindow* window, int button, int action, int mods)
  {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    switch (action) {
    case GLFW_PRESS:
      switch (button) {
      case 0: viewer.startRotation(float(x), float(y)); break;
      case 1: viewer.startZoom(float(x), float(y)); break;
      case 2: viewer.startPan(float(x), float(y)); break;
      }
      break;
    case GLFW_RELEASE:
      viewer.stopAction();
      break;
    }
  }
 
  void scrollFunc(GLFWwindow* window, double x, double y)
  {
    float speed = 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      speed = 0.1f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
      speed = 0.01f;
    }

    bool distance = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;

    viewer.dolly(float(x), float(y), speed, distance);
  }

  

  void runObjReader(Logger logger, std::string path)
  {
    auto time0 = std::chrono::high_resolution_clock::now();
    Mesh* mesh = nullptr;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      logger(2, "Failed to open file %s: %d", path.c_str(), GetLastError());
    }
    else {
      DWORD hiSize;
      DWORD loSize = GetFileSize(h, &hiSize);
      size_t fileSize = (size_t(hiSize) << 32u) + loSize;

      HANDLE m = CreateFileMappingA(h, 0, PAGE_READONLY, 0, 0, NULL);
      if (m == INVALID_HANDLE_VALUE) {
        logger(2, "Failed to map file %s: %d", path.c_str(), GetLastError());
      }
      else {
        const void * ptr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
        if (ptr == nullptr) {
          logger(2, "Failed to map view of file %s: %d", path.c_str(), GetLastError());
        }
        else {
          mesh = readObj(logger, ptr, fileSize);
          UnmapViewOfFile(ptr);
        }
        CloseHandle(m);
      }
      CloseHandle(h);
    }
    if (mesh) {

      auto * name = path.c_str();
      for (auto * t = name; *t != '\0'; t++) {
        if (*t == '\\' || *t == '//') name = t + 1;
      }
      mesh->name = mesh->strings.intern(name);

      auto time1 = std::chrono::high_resolution_clock::now();
      auto e = std::chrono::duration_cast<std::chrono::milliseconds>((time1 - time0)).count();
      std::lock_guard<std::mutex> guard(incomingMeshLock);
      incomingMeshes.push_back(mesh);
      logger(0, "Read %s in %lldms", path.c_str(), e);
    }
    else {
      logger(0, "Failed to read %s", path.c_str());
    }
  }

  void guiTraversal(GLFWwindow* window)
  {
    float menuHeight = 0.f;
    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", nullptr, nullptr)) { glfwSetWindowShouldClose(window, true); }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("View all", nullptr, nullptr)) { viewer.viewAll(); }
        if (ImGui::MenuItem("Solid", nullptr, &solid)) {}
        ImGui::EndMenu();
      }
      menuHeight = ImGui::GetWindowSize().y;
      ImGui::EndMainMenuBar();
    }


    {
      ImGui::SetNextWindowPos(ImVec2(0, menuHeight));
      ImGui::SetNextWindowSize(ImVec2(leftSplit, height - menuHeight));
      ImGui::Begin("##left", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);


      //ImGuiContext& g = *GImGui;
      //ImGuiWindow* window = g.CurrentWindow;

      ImVec2 backup_pos = ImGui::GetCursorPos();
      ImGui::SetCursorPosX(leftSplit - thickness);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.10f));
      ImGui::Button("##Splitter", ImVec2(thickness, -1));

      ImGui::PopStyleColor(3);
      if (ImGui::IsItemActive()) {
        leftSplit += ImGui::GetIO().MouseDelta.x;
        if (leftSplit < 50) leftSplit = 50;
        if (width - thickness < leftSplit) leftSplit = width - thickness;
      }

      ImGui::SetCursorPos(backup_pos);
      ImGui::BeginChild("Meshes", ImVec2(leftSplit - thickness - backup_pos.x, -1), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
      for(auto & item : meshItems) {
        auto * m = item.mesh;
        if (ImGui::TreeNodeEx(m, ImGuiTreeNodeFlags_DefaultOpen, "%s Vn=%d Tn=%d", m->name ? m->name : "unnamed", m->vtx_n, m->tri_n)) {
          for (unsigned o = 0; o < m->obj_n; o++) {
            if (ImGui::Button(m->obj[o])) {

            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::EndChild();
      ImGui::End();
    }
    //ImGui::ShowDemoWindow();
  }

  void checkQueues()
  {
    std::list<Mesh*> newMeshes;
    {
      std::lock_guard<std::mutex> guard(incomingMeshLock);
      if (!incomingMeshes.empty()) {
        newMeshes = std::move(incomingMeshes);
        incomingMeshes.clear();
      }
    }
    if (!newMeshes.empty()) {
      for (auto & m : newMeshes) {
        meshItems.push_back({ m, renderer->createRenderMesh(m) });
      }
      auto bbox = createEmptyBBox3f();
      for (auto & item : meshItems) {
        auto * m = item.mesh;
        if (isEmpty(m->bbox)) continue;
        engulf(bbox, m->bbox);
      }
      if (isEmpty(bbox)) {
        bbox = BBox3f(Vec3f(-1.f), Vec3f(1.f));
      }
      viewer.setViewVolume(bbox);
      viewer.viewAll();
    }
  }

}



int main(int argc, char** argv)
{
  GLFWwindow* window;
  tasks.init(logger);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return -1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  if (!glfwVulkanSupported()) {
    logger(2, "GLFW: Vulkan not supported");
    return -1;
  }
  glfwGetFramebufferSize(window, &width, &height);

  VkInstance instance;
  createInstance(instance);

  VkPhysicalDevice physicalDevice;
  choosePhysicalDevice(physicalDevice, instance);

  VkDevice device;
  VkQueue queue;
  uint32_t queueFamilyIndex;
  createDevice(device, queue, queueFamilyIndex, instance, physicalDevice);

  VkDescriptorPool descPool;
  createDescriptorPool(descPool, device);


  VkCommandBuffer cmdBuf;
  VkCommandPool cmdPool;
  createCommandBuffer(cmdBuf, cmdPool, queueFamilyIndex, device);

  initSwapChain(window, instance, physicalDevice, queueFamilyIndex);


  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(window, true);

  ImGui_ImplVulkanH_CreateWindowDataCommandBuffers(physicalDevice, device, queueFamilyIndex, &imguiWindowData, nullptr);
  ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(physicalDevice, device, &imguiWindowData, nullptr, width, height);

#if 0
  struct ImGui_ImplVulkanH_WindowData
  {
    int                 Width;
    int                 Height;
    VkSwapchainKHR      Swapchain;
    VkSurfaceKHR        Surface;
    VkSurfaceFormatKHR  SurfaceFormat;
    VkPresentModeKHR    PresentMode;
    VkRenderPass        RenderPass;
    bool                ClearEnable;
    VkClearValue        ClearValue;
    uint32_t            BackBufferCount;
    VkImage             BackBuffer[16];
    VkImageView         BackBufferView[16];
    VkFramebuffer       Framebuffer[16];
    uint32_t            FrameIndex;
    ImGui_ImplVulkanH_FrameData Frames[IMGUI_VK_QUEUED_FRAMES];

    IMGUI_IMPL_API ImGui_ImplVulkanH_WindowData();
  };
#endif


  vCtx = new VulkanContext(logger, physicalDevice, device);
  {
    auto h = vCtx->createFrameBuffer();
    auto w = h;
  }

  renderer = new Renderer(logger, vCtx, imguiWindowData.BackBufferView, imguiWindowData.BackBufferCount, width, height);


  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = instance;
  init_info.PhysicalDevice = physicalDevice;
  init_info.Device = device;
  init_info.QueueFamily = queueFamilyIndex;
  init_info.Queue = queue;
  init_info.PipelineCache = nullptr;
  init_info.DescriptorPool = descPool;
  init_info.Allocator = nullptr;
  init_info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&init_info, imguiWindowData.RenderPass);
  uploadFonts(device, queue);

  glfwSetWindowSizeCallback(window, resizeFunc);
  glfwSetCursorPosCallback(window, moveFunc);
  glfwSetMouseButtonCallback(window, buttonFunc);
  glfwSetScrollCallback(window, scrollFunc);

  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  ImGui_ImplGlfw_InitForOpenGL(window, false);
  //ImGui_ImplOpenGL2_Init();

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  BBox3f viewVolume(Vec3f(-2.f), Vec3f(2.f));
  viewer.setViewVolume(viewVolume);
  viewer.resize(1280, 720);



  for (int i = 1; i < argc; i++) {
    auto arg = std::string(argv[i]);
    if (arg.substr(0, 2) == "--") {
    }
    else {
      auto argLower = arg;
      for(auto & c :argLower) c = std::tolower(c);
      auto l = argLower.rfind(".obj");
      if (l != std::string::npos) {
        TaskFunc taskFunc = [arg]() {runObjReader(logger, arg); };
        tasks.enqueue(taskFunc);
      }
    }
  }

  glfwGetFramebufferSize(window, &width, &height);
  leftSplit = 0.25f*width;
  while (!glfwWindowShouldClose(window))
  {
    vCtx->houseKeep();

    tasks.update();
    checkQueues();
    glfwGetFramebufferSize(window, &width, &height);
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    guiTraversal(window);
    ImGui::Render();

#if 0

    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    {
      Vec2f viewerPos(leftSplit, menuHeight);
      Vec2f viewerSize(width - viewerPos.x, height - viewerPos.y);
      glViewport(int(viewerPos.x), int(viewerPos.y), int(viewerSize.x), int(viewerSize.y));
      viewer.resize(viewerPos, viewerSize);

      glEnable(GL_DEPTH_TEST);

      viewer.update();

      glMatrixMode(GL_PROJECTION);
      glLoadMatrixf(viewer.getProjectionMatrix().data);


      glMatrixMode(GL_MODELVIEW);
      glLoadMatrixf(viewer.getViewMatrix().data);

      if (solid) {
        glPolygonOffset(1.f, 1.f);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glColor3f(0.2f, 0.2f, 0.6f);
        for (auto * m : meshes) {
          glBegin(GL_TRIANGLES);
          for (unsigned i = 0; i < 3 * m->tri_n; i++) {
            glVertex3fv(m->vtx[m->tri[i]].data);
          }
          glEnd();
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glColor3f(1.f, 1.f, 1.f);
      for (auto * m : meshes) {
        glBegin(GL_TRIANGLES);
        for (unsigned i = 0; i < 3 * m->tri_n; i++) {
          glVertex3fv(m->vtx[m->tri[i]].data);
        }
        glEnd();
      }
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
#endif

    frameRender(device, queue);
    framePresent(queue);
    glfwPollEvents();
  }

  auto rv = vkDeviceWaitIdle(device);
  assert(rv == VK_SUCCESS);

  for (auto & item : meshItems) {
    renderer->destroyRenderMesh(item.renderMesh);
  }
  meshItems.clear();

  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  delete renderer;

  vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuf);
  vkDestroyCommandPool(device, cmdPool, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);

  glfwDestroyWindow(window);

  glfwTerminate();

  return 0;
}
