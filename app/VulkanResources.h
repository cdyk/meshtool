#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"

class VulkanContext;

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
  VkDescriptorBufferInfo descInfo;
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


struct Image : ResourceBase
{
  Image(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  VkFormat format;
};
typedef ResourceHandle<Image> ImageHandle;

struct ImageView : ResourceBase
{
  ImageView(ResourceManagerBase& manager) : ResourceBase(manager) {}
  ImageHandle image;
  VkImageView view;
};
typedef ResourceHandle<ImageView> ImageViewHandle;

struct Sampler : ResourceBase
{
  Sampler(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkSampler sampler = VK_NULL_HANDLE;
};
typedef ResourceHandle<Sampler> SamplerHandle;

struct FrameBuffer : ResourceBase
{
  FrameBuffer(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkFramebuffer fb = VK_NULL_HANDLE;
  RenderPassHandle pass;
  Vector<ImageViewHandle> attachments;
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
  SwapChain(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkSwapchainKHR swapChain = VK_NULL_HANDLE;
};
typedef ResourceHandle<SwapChain> SwapChainHandle;

struct AccelerationStructure : public ResourceBase
{
  AccelerationStructure(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkAccelerationStructureNVX acc = VK_NULL_HANDLE;
  VkDeviceMemory structureMem = VK_NULL_HANDLE;
  VkDeviceMemory scratchMem = VK_NULL_HANDLE;
  VkBuffer scratchBuffer = VK_NULL_HANDLE;
};
typedef ResourceHandle<AccelerationStructure> AccelerationStructureHandle;


class VulkanContext;

class VulkanResources
{
public:
  VulkanResources(VulkanContext* vCtx, Logger logger) : vCtx(vCtx), logger(logger) {}

  ~VulkanResources();

  void init();

  void houseKeep();

  PipelineHandle createPipeline();

  PipelineHandle createPipeline(Vector<VkVertexInputBindingDescription>& inputBind,
                                Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                VkPipelineLayoutCreateInfo& pipelineLayoutInfo,
                                VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                RenderPassHandle renderPass,
                                ShaderHandle shader,
                                const VkPipelineRasterizationStateCreateInfo& rasterizationInfo,
                                VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  RenderBufferHandle createBuffer();
  RenderBufferHandle createBuffer(size_t initialSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags properties);
  RenderBufferHandle createStagingBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); }
  RenderBufferHandle createVertexDeviceBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); }
  RenderBufferHandle createUniformBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); }

  DescriptorSetHandle createDescriptorSet(VkDescriptorSetLayout descLayout);

  ShaderHandle createShader(Vector<ShaderInputSpec>& spec, const char* name = nullptr);
  RenderPassHandle createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                    VkSubpassDescription* subpasses, uint32_t subpassCount,
                                    VkSubpassDependency* dependency);
  //RenderImageHandle wrapRenderImageView(VkImageView view);


  ImageHandle createImage(VkImageCreateInfo& imageCreateInfo);
  ImageHandle createImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format);

  ImageViewHandle createImageView(VkImage image, VkFormat format, VkImageSubresourceRange subResRange);
  ImageViewHandle createImageView(ImageHandle, VkImageViewCreateInfo& imageViewCreateInfo);

  SamplerHandle createSampler(VkSamplerCreateInfo& samplerCreateInfo);

  FrameBufferHandle createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<ImageViewHandle>& attachments);
  CommandPoolHandle createCommandPool(uint32_t queueFamilyIx);
  CommandBufferHandle createPrimaryCommandBuffer(CommandPoolHandle pool);
  FenceHandle createFence(bool signalled);
  SemaphoreHandle createSemaphore();
  SwapChainHandle createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo);

  AccelerationStructureHandle createAccelerationStructure();

  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);

private:
  VulkanContext* vCtx = nullptr;
  Logger logger = nullptr;

  void destroyBuffer(RenderBuffer*);
  void destroyDescriptorSet(DescriptorSet*);
  void destroyShader(Shader* shader);
  void destroyPipeline(Pipeline*);
  void destroyRenderPass(RenderPass*);
  void destroyFrameBuffer(FrameBuffer*);
  void destroyRenderImage(Image*);
  void destroyImageView(ImageView*);
  void destroySampler(Sampler*);
  void destroyCommandPool(CommandPool*);
  void destroyCommandBuffer(CommandBuffer*);
  void destroyFence(Fence*);
  void destroySemaphore(Semaphore*);
  void destroySwapChain(SwapChain*);
  void destroyAccelerationStructure(AccelerationStructure*);


  ResourceManager<RenderBuffer> bufferResources;
  ResourceManager<DescriptorSet> descriptorSetResources;
  ResourceManager<Shader> shaderResources;
  ResourceManager<Pipeline> pipelineResources;
  ResourceManager<RenderPass> renderPassResources;
  ResourceManager<FrameBuffer> frameBufferResources;
  ResourceManager<Image> renderImageResources;
  ResourceManager<ImageView> imageViewResources;
  ResourceManager<Sampler> samplerResources;
  ResourceManager<CommandPool> commandPoolResources;
  ResourceManager<CommandBuffer> commandBufferResources;
  ResourceManager<Fence> fenceResources;
  ResourceManager<Semaphore> semaphoreResources;
  ResourceManager<SwapChain> swapChainResources;
  ResourceManager<AccelerationStructure> accelerationStructures;

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