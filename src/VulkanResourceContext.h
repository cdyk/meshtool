#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"
#include "VulkanContext.h"

struct ShaderInputSpec {
  uint32_t* spv;
  size_t siz;
  VkShaderStageFlagBits stage;
};

struct DescriptorSet : ResourceBase
{
  DescriptorSet(ResourceManagerBase& manager): ResourceBase(manager) {}
  VkDescriptorSet descSet = VK_NULL_HANDLE;
};
typedef ResourceHandle<DescriptorSet> DescriptorSetHandle;

struct Shader : ResourceBase
{
  Shader(ResourceManagerBase& manager) : ResourceBase(manager) {}
  Vector<VkPipelineShaderStageCreateInfo> stageCreateInfo;
};
typedef ResourceHandle<Shader> ShaderHandle;


struct RenderBuffer : ResourceBase
{
  RenderBuffer(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  size_t requestedSize = 0;
  size_t alignedSize = 0;
};
typedef ResourceHandle<RenderBuffer> RenderBufferHandle;

struct RenderPass : ResourceBase
{
  RenderPass(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkRenderPass pass = VK_NULL_HANDLE;
};
typedef ResourceHandle<RenderPass> RenderPassHandle;

struct Pipeline : ResourceBase
{
  Pipeline(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkPipeline pipe = VK_NULL_HANDLE;
  VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
  RenderPassHandle pass;
};
typedef ResourceHandle<Pipeline> PipelineHandle;


struct RenderImage : ResourceBase
{
  RenderImage(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkImageView view;
  VkFormat format;
};
typedef ResourceHandle<RenderImage> RenderImageHandle;

struct FrameBuffer : ResourceBase
{
  FrameBuffer(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkFramebuffer fb = VK_NULL_HANDLE;
  RenderPassHandle pass;
  Vector<RenderImageHandle> attachments;
};
typedef ResourceHandle<FrameBuffer> FrameBufferHandle;

struct CommandPool : ResourceBase
{
  CommandPool(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkCommandPool cmdPool = VK_NULL_HANDLE;
};
typedef ResourceHandle<CommandPool> CommandPoolHandle;

struct CommandBuffer : ResourceBase
{
  CommandBuffer(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
  CommandPoolHandle pool;
};
typedef ResourceHandle<CommandBuffer> CommandBufferHandle;

struct Fence : ResourceBase
{
  Fence(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkFence fence = VK_NULL_HANDLE;
};
typedef ResourceHandle<Fence> FenceHandle;

struct Semaphore : ResourceBase
{
  Semaphore(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkSemaphore semaphore = VK_NULL_HANDLE;
};
typedef ResourceHandle<Semaphore> SemaphoreHandle;

struct SwapChain : public ResourceBase
{
  VkSwapchainKHR swapChain = VK_NULL_HANDLE;
};
typedef ResourceHandle<SwapChain> SwapChainHandle;


class VulkanResourceContext : public VulkanContext
{
public:
  VulkanResourceContext() = delete;

  VulkanResourceContext(Logger logger, const char**instanceExts, uint32_t instanceExtCount);

  virtual ~VulkanResourceContext();

  virtual void init();

  virtual void houseKeep();


  PipelineHandle createPipeline(Vector<VkVertexInputBindingDescription>& inputBind,
                                Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                VkPipelineLayoutCreateInfo& pipelineLayoutInfo,
                                VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                RenderPassHandle renderPass,
                                ShaderHandle shader,
                                const VkPipelineRasterizationStateCreateInfo& rasterizationInfo,
                                VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  RenderBufferHandle createBuffer(size_t initialSize, VkImageUsageFlags usageFlags);
  RenderBufferHandle createVertexBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); }
  RenderBufferHandle createUniformBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT); }

  DescriptorSetHandle createDescriptorSet(VkDescriptorSetLayout descLayout);

  ShaderHandle createShader(Vector<ShaderInputSpec>& spec, const char* name = nullptr);
  RenderPassHandle createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                    VkSubpassDescription* subpasses, uint32_t subpassCount,
                                    VkSubpassDependency* dependency);
  RenderImageHandle wrapRenderImageView(VkImageView view);

  RenderImageHandle createRenderImage(VkImage image, VkFormat format, VkImageSubresourceRange subResRange);

  RenderImageHandle createRenderImage(VkImageCreateInfo& imageCreateInfo);

  RenderImageHandle createRenderImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format);
  FrameBufferHandle createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<RenderImageHandle>& attachments);
  CommandPoolHandle createCommandPool(uint32_t queueFamilyIx);
  CommandBufferHandle createPrimaryCommandBuffer(CommandPoolHandle pool);
  FenceHandle createFence(bool signalled);
  SemaphoreHandle createSemaphore();
  SwapChainHandle createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo);


private:
  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);

  void destroyBuffer(RenderBuffer*);
  void destroyDescriptorSet(DescriptorSet*);
  void destroyShader(Shader* shader);
  void destroyPipeline(Pipeline*);
  void destroyRenderPass(RenderPass*);
  void destroyFrameBuffer(FrameBuffer*);
  void destroyRenderImage(RenderImage*);
  void destroyCommandPool(CommandPool*);
  void destroyCommandBuffer(CommandBuffer*);
  void destroyFence(Fence*);
  void destroySemaphore(Semaphore*);
  void destroySwapChain(SwapChain*);


  ResourceManager<RenderBuffer> bufferResources;
  ResourceManager<DescriptorSet> descriptorSetResources;
  ResourceManager<Shader> shaderResources;
  ResourceManager<Pipeline> pipelineResources;
  ResourceManager<RenderPass> renderPassResources;
  ResourceManager<FrameBuffer> frameBufferResources;
  ResourceManager<RenderImage> renderImageResources;
  ResourceManager<CommandPool> commandPoolResources;
  ResourceManager<CommandBuffer> commandBufferResources;
  ResourceManager<Fence> fenceResources;
  ResourceManager<Semaphore> semaphoreResources;
  ResourceManager<SwapChain> swapChainResources;

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

struct DebugScope : NonCopyable
{
  VulkanContext* vCtx;
  VkCommandBuffer cmdBuf;

  DebugScope(VulkanContext* vCtx, VkCommandBuffer cmdBuf, const char* name);
  ~DebugScope();
};