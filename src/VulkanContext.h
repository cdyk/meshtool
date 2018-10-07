#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanInfos.h"

struct ShaderInputSpec {
  uint32_t* spv;
  size_t siz;
  VkShaderStageFlagBits stage;
};

struct DescriptorSet : ResourceBase
{
  VkDescriptorSet descSet = VK_NULL_HANDLE;
};
typedef ResourceHandle<DescriptorSet> DescriptorSetHandle;

struct RenderFence : ResourceBase
{
  VkFence fence = VK_NULL_HANDLE;
};
typedef ResourceHandle<RenderFence> RenderFenceHandle;


struct Shader : ResourceBase
{
  Vector<VkPipelineShaderStageCreateInfo> stageCreateInfo;
};
typedef ResourceHandle<Shader> ShaderHandle;


struct RenderBuffer : ResourceBase
{
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  size_t requestedSize = 0;
  size_t alignedSize = 0;
};
typedef ResourceHandle<RenderBuffer> RenderBufferHandle;

struct RenderPass : ResourceBase
{
  VkRenderPass pass = VK_NULL_HANDLE;
};
typedef ResourceHandle<RenderPass> RenderPassHandle;

struct Pipeline : ResourceBase
{
  VkPipeline pipe = VK_NULL_HANDLE;
  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
  RenderPassHandle pass;
};
typedef ResourceHandle<Pipeline> PipelineHandle;


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


  PipelineHandle createPipeline(Vector<VkVertexInputBindingDescription>& inputBind,
                                Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                VkPipelineLayoutCreateInfo& pipelineLayoutInfo,
                                VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                RenderPassHandle renderPass,
                                ShaderHandle shader,
                                const VkPipelineRasterizationStateCreateInfo& rasterizationInfo,
                                VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);


  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);

  RenderFenceHandle createFence(bool signalled);



  RenderBufferHandle createBuffer(size_t initialSize, VkImageUsageFlags usageFlags);
  RenderBufferHandle createVertexBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); }
  RenderBufferHandle createUniformBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT); }

  DescriptorSetHandle createDescriptorSet(VkDescriptorSetLayout descLayout);
  void updateDescriptorSet(DescriptorSetHandle descriptorSet, RenderBufferHandle buffer);

  ShaderHandle createShader(Vector<ShaderInputSpec>& spec, const char* name = nullptr);
  RenderPassHandle createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                    VkSubpassDescription* subpasses, uint32_t subpassCount);
  RenderImageHandle wrapRenderImageView(VkImageView view);
  RenderImageHandle createRenderImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format);
  FrameBufferHandle createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<RenderImageHandle>& attachments);

  VulkanInfos infos;
  Logger logger = nullptr;
  VkInstance instance = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;
  VkDescriptorPool descPool = VK_NULL_HANDLE;
  //VkCommandPool cmdPool = VK_NULL_HANDLE;
  //VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

  VkPhysicalDeviceProperties physicalDeviceProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

  PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
  PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
  PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;

private:
  bool debugLayer = true;
  VkDebugUtilsMessengerEXT debugCallbackHandle;

  void annotate(VkDebugReportObjectTypeEXT type, uint64_t object, const char* name);

  void destroyFence(RenderFence*);
  void destroyBuffer(RenderBuffer*);
  void destroyDescriptorSet(DescriptorSet*);
  void destroyShader(Shader* shader);
  void destroyPipeline(Pipeline*);
  void destroyRenderPass(RenderPass*);
  void destroyFrameBuffer(FrameBuffer*);
  void destroyRenderImage(RenderImage*);

  ResourceManager<RenderFence> fenceResources;
  ResourceManager<RenderBuffer> bufferResources;
  ResourceManager<DescriptorSet> descriptorSetResources;
  ResourceManager<Shader> shaderResources;
  ResourceManager<Pipeline> pipelineResources;
  ResourceManager<RenderPass> renderPassResources;
  ResourceManager<FrameBuffer> frameBufferResources;
  ResourceManager<RenderImage> renderImageResources;
};

struct MappedBufferBase
{
  MappedBufferBase() = delete;
  MappedBufferBase(const MappedBufferBase&) = delete;

  VulkanContext* vCtx;
  RenderBufferHandle h;
  MappedBufferBase(void** ptr, VulkanContext* vCtx, RenderBufferHandle h);
  ~MappedBufferBase();

};

template<typename T>
struct MappedBuffer : MappedBufferBase
{
  MappedBuffer(VulkanContext* vCtx, RenderBufferHandle h) : MappedBufferBase((void**)&mem, vCtx, h) {}

  T* mem;
};

struct DebugScope
{
  VulkanContext* vCtx;
  VkCommandBuffer cmdBuf;

  DebugScope() = delete;
  DebugScope(const DebugScope&) = delete;
  DebugScope(VulkanContext* vCtx, VkCommandBuffer cmdBuf, const char* name);
  ~DebugScope();
};