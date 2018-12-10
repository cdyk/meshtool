#pragma once
#include <vulkan/vulkan.h>
#include "Common.h"
#include "ResourceManager.h"

class VulkanContext;

struct DescriptorSet : ResourceBase
{
  DescriptorSet(ResourceManagerBase& manager): ResourceBase(manager) {}
  VkDescriptorSet descSet = VK_NULL_HANDLE;
};
typedef ResourceHandle<DescriptorSet> DescriptorSetHandle;

struct DescriptorPool : ResourceBase
{
  DescriptorPool(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkDescriptorPool pool = VK_NULL_HANDLE;
};
typedef ResourceHandle<DescriptorPool> DescriptorPoolHandle;


struct Shader : ResourceBase
{
  Shader(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkPipelineShaderStageCreateInfo stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
};
typedef ResourceHandle<Shader> ShaderHandle;


struct Buffer : ResourceBase
{
  Buffer(ResourceManagerBase& manager) : ResourceBase(manager) {}
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory mem = VK_NULL_HANDLE;
  void * hostPtr = nullptr;
  size_t requestedSize = 0;
  size_t alignedSize = 0;
  VkDescriptorBufferInfo descInfo;
};
typedef ResourceHandle<Buffer> RenderBufferHandle;

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
  VkAccelerationStructureNV acc = VK_NULL_HANDLE;
  VkDeviceMemory structureMem = VK_NULL_HANDLE;
  VkMemoryRequirements scratchReqs{ 0 };
};
typedef ResourceHandle<AccelerationStructure> AccelerationStructureHandle;


class VulkanContext;

class VulkanResources
{
public:
  VulkanResources(VulkanContext* vCtx, Logger logger);

  ~VulkanResources();

  void init();

  void houseKeep();

  PipelineHandle createPipeline();

  PipelineHandle createPipeline(const Vector<VkVertexInputBindingDescription>& inputBind,
                                const Vector<VkVertexInputAttributeDescription>& inputAttrib,
                                VkPipelineLayoutCreateInfo& pipelineLayoutInfo,
                                VkDescriptorSetLayoutCreateInfo& descLayoutInfo,
                                RenderPassHandle renderPass,
                                Vector<ShaderHandle> shaders,
                                const VkPipelineRasterizationStateCreateInfo& rasterizationInfo,
                                VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

  RenderBufferHandle createBuffer();
  RenderBufferHandle createBuffer(size_t initialSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags properties);
  RenderBufferHandle createStagingBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); }
  RenderBufferHandle createVertexDeviceBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); }
  RenderBufferHandle createIndexDeviceBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); }
  RenderBufferHandle createUniformBuffer(size_t initialSize) { return createBuffer(initialSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); }
  RenderBufferHandle createStorageBuffer(size_t size) { return createBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); }

  DescriptorSetHandle createDescriptorSet(VkDescriptorSetLayout descLayout);

  ShaderHandle createShader(uint32_t* spv, size_t siz);
  RenderPassHandle createRenderPass(VkAttachmentDescription* attachments, uint32_t attachmentCount,
                                    VkSubpassDescription* subpasses, uint32_t subpassCount,
                                    VkSubpassDependency* dependency);

  ImageHandle createImage(VkImageCreateInfo& imageCreateInfo);
  ImageHandle createImage(uint32_t w, uint32_t h, VkImageUsageFlags usageFlags, VkFormat format);

  ImageViewHandle createImageView(VkImage image, VkFormat format, VkImageSubresourceRange subResRange);
  ImageViewHandle createImageView(ImageHandle, VkImageViewCreateInfo& imageViewCreateInfo);

  SamplerHandle createSampler(VkSamplerCreateInfo& samplerCreateInfo);

  DescriptorPoolHandle createDescriptorPool(const VkDescriptorPoolCreateInfo* info);

  FrameBufferHandle createFrameBuffer(RenderPassHandle pass, uint32_t w, uint32_t h, Vector<ImageViewHandle>& attachments);
  CommandPoolHandle createCommandPool(uint32_t queueFamilyIx);
  FenceHandle createFence(bool signalled);
  SemaphoreHandle createSemaphore();
  SwapChainHandle createSwapChain(SwapChainHandle oldSwapChain, VkSwapchainCreateInfoKHR& swapChainInfo);

  AccelerationStructureHandle createAccelerationStructure();
  AccelerationStructureHandle createAccelerationStructure(VkAccelerationStructureTypeNV type, uint32_t geometryCount, VkGeometryNV* geometries, uint32_t instanceCount);

  bool getMemoryTypeIndex(uint32_t& index, uint32_t typeBits, uint32_t requirements);
  uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags requirements);

  void copyHostMemToBuffer(RenderBufferHandle buffer, void* src, size_t size);

private:
  VulkanContext* vCtx = nullptr;
  Logger logger = nullptr;

  ResourceManager<Buffer> bufferResources;
  ResourceManager<DescriptorSet> descriptorSetResources;
  ResourceManager<DescriptorPool> descriptorPoolResources;
  ResourceManager<Shader> shaderResources;
  ResourceManager<Pipeline> pipelineResources;
  ResourceManager<RenderPass> renderPassResources;
  ResourceManager<FrameBuffer> frameBufferResources;
  ResourceManager<Image> renderImageResources;
  ResourceManager<ImageView> imageViewResources;
  ResourceManager<Sampler> samplerResources;
  ResourceManager<CommandPool> commandPoolResources;
  ResourceManager<Fence> fenceResources;
  ResourceManager<Semaphore> semaphoreResources;
  ResourceManager<SwapChain> swapChainResources;
  ResourceManager<AccelerationStructure> accelerationStructures;
};
