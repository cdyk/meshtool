#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"

struct ShaderInputSpec {
  uint32_t* spv;
  size_t siz;
  VkShaderStageFlagBits stage;
};



struct RenderShader
{
  Buffer<VkPipelineShaderStageCreateInfo> stageCreateInfo;
};

struct RenderBuffer
{
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  size_t size = 0;
};

struct RenderPass : ResourceBase
{
  VkRenderPass pass = VK_NULL_HANDLE;
};
typedef ResourceHandle<RenderPass> RenderPassHandle;


struct RenderImage : ResourceBase
{
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkImageView view;
  VkFormat format;
};
typedef ResourceHandle<RenderImage> RenderImageHandle;

struct FrameBuffer : ResourceBase
{
  VkFramebuffer fb = VK_NULL_HANDLE;
  RenderPassHandle pass;
  Vector<RenderImageHandle> attachments;
};
typedef ResourceHandle<FrameBuffer> FrameBufferHandle;

struct VulkanContext
{
  VulkanContext() = delete;

  VulkanContext(Logger logger,
                const char**instanceExts, uint32_t instanceExtCount,
                int hasPresentationSupport(VkInstance, VkPhysicalDevice, uint32_t queueFamily));
  ~VulkanContext();

  void houseKeep();

  void buildShader(RenderShader& shader, ShaderInputSpec* spec, unsigned N);
  void destroyShader(RenderShader& shader);

  void buildPipeline(VkPipeline& pipeline,
                     VkDevice device,
                     Buffer<VkPipelineShaderStageCreateInfo>& shaders,
                     Buffer<VkVertexInputBindingDescription>& inputBind,
                     Buffer<VkVertexInputAttributeDescription>& inputAttrib,
                     VkPipelineLayout pipelineLayout,
                     VkRenderPass renderPass,
                     VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                     VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                     VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);

  RenderBuffer createBuffer(size_t initialSize, VkImageUsageFlags usageFlags);
  RenderBuffer createVertexBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); }

  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);

  RenderPassHandle createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                    VkSubpassDescription* subpasses, uint32_t subpassCount);
  RenderImageHandle wrapRenderImageView(VkImageView view);
  RenderImageHandle createRenderImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format);
  FrameBufferHandle createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<RenderImageHandle>& attachments);

  Logger logger = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

private:
  void destroyRenderPass(RenderPass*);
  void destroyRenderImage(RenderImage*);

  ResourceManager<RenderPass> renderPassResources;
  ResourceManager<FrameBuffer> frameBufferResources;
  ResourceManager<RenderImage> renderImageResources;
};