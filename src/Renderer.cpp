#include <cassert>
#include "Common.h"
#include "Renderer.h"
#include "Mesh.h"
#include "VulkanContext.h"


struct RenderMesh
{
  RenderBuffer vtxNrmTex;
  uint32_t tri_n = 0;
};

struct RenderPipeline
{
  VkPipeline pipeline;
  RenderShader shader;
  Buffer<VkDescriptorSetLayout> descSetLayout;
  VkPipelineLayout pipelineLayout;
};

namespace {

  uint32_t vanillaVS[]
#include "vanillaVS.glsl.h"
    ;

  uint32_t vanillaPS[]
#include "vanillaPS.glsl.h"
    ;

  void createVanillaVertexInputDesc(Buffer<VkVertexInputBindingDescription>& inputBind,
                                    Buffer<VkVertexInputAttributeDescription>& inputAttrib)
  {
    inputBind.accommodate(1);
    inputBind[0].binding = 0;
    inputBind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    inputBind[0].stride = sizeof(Vec3f) + sizeof(Vec3f) + sizeof(Vec2f);

    inputAttrib.accommodate(3);
    inputAttrib[0].binding = 0;
    inputAttrib[0].location = 0;
    inputAttrib[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[0].offset = 0;
    inputAttrib[1].binding = 0;
    inputAttrib[1].location = 1;
    inputAttrib[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[1].offset = sizeof(Vec3f);
    inputAttrib[2].binding = 0;
    inputAttrib[2].location = 2;
    inputAttrib[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    inputAttrib[2].offset = sizeof(Vec3f) + sizeof(Vec3f);
  }

  void createVanillaPiplineLayout(VkPipelineLayout& pipelineLayout, Buffer<VkDescriptorSetLayout>& descSetLayout, VkDevice device)
  {
    VkDescriptorSetLayoutBinding descSetLayoutBind = {};
    descSetLayoutBind.binding = 0;
    descSetLayoutBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetLayoutBind.descriptorCount = 1;
    descSetLayoutBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    descSetLayoutBind.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo descLayoutCI = {};
    descLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descLayoutCI.pNext = NULL;
    descLayoutCI.bindingCount = 1;
    descLayoutCI.pBindings = &descSetLayoutBind;

    descSetLayout.accommodate(1);
    auto rv = vkCreateDescriptorSetLayout(device, &descLayoutCI, nullptr, descSetLayout.data());
    assert(rv == VK_SUCCESS);

    VkPipelineLayoutCreateInfo pipeLayoutCI = {};
    pipeLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLayoutCI.pNext = NULL;
    pipeLayoutCI.pushConstantRangeCount = 0;
    pipeLayoutCI.pPushConstantRanges = NULL;
    pipeLayoutCI.setLayoutCount = uint32_t(descSetLayout.getCount());
    pipeLayoutCI.pSetLayouts = descSetLayout.data();

    rv = vkCreatePipelineLayout(device, &pipeLayoutCI, NULL, &pipelineLayout);
    assert(rv == VK_SUCCESS);
  }

  


#if 0
  void createRenderPass(RenderPass& renderPass, VkDevice device, VkSwapchainKHR swapChain )
  {
    VkFormat format;  // from wd->surfaceFormat.format

    VkSemaphoreCreateInfo imgAcqSemCI;
    imgAcqSemCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    imgAcqSemCI.pNext = NULL;
    imgAcqSemCI.flags = 0;

    auto rv = vkCreateSemaphore(device, &imgAcqSemCI, NULL, &renderPass.imgAcqSem);
    assert(rv == VK_SUCCESS);

    // Acquire the swapchain image in order to set its layout
    uint32_t bufferIndex;
    rv = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, renderPass.imgAcqSem, VK_NULL_HANDLE, &bufferIndex);
    assert(rv == VK_SUCCESS);

    VkAttachmentDescription attachments[2];
    attachments[0].format = format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags = 0;

    attachments[1].format = info.depth.format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags = 0;

    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depth_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;

    res = vkCreateRenderPass(info.device, &rp_info, NULL, &info.render_pass);
    assert(res == VK_SUCCESS);
  }
#endif
}


Renderer::Renderer(Logger logger, VkPhysicalDevice physicalDevice, VkDevice device, VkImageView* backBuffers, uint32_t backBufferCount, uint32_t w, uint32_t h) :
  logger(logger),
  vkCtx(new VulkanContext(logger, physicalDevice, device))
{

  depthImage = new RenderImage;
  vkCtx->createDepthImage(*depthImage, w, h);

  

  //for (i = 0; i < info.swapchainImageCount; i++) {
  //  vkDestroyFramebuffer(info.device, info.framebuffers[i], NULL);
  //}
  //free(info.framebuffers);



  ShaderInputSpec stages[] = {
    {vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_VERTEX_BIT},
    {vanillaPS, sizeof(vanillaPS), VK_SHADER_STAGE_FRAGMENT_BIT}
  };
  vanillaPipeline = new RenderPipeline;
  vkCtx->buildShader(vanillaPipeline->shader, stages, 2);

  Buffer<VkVertexInputBindingDescription> inputBind;
  Buffer<VkVertexInputAttributeDescription> inputAttrib;
  createVanillaVertexInputDesc(inputBind, inputAttrib);

  createVanillaPiplineLayout(vanillaPipeline->pipelineLayout, vanillaPipeline->descSetLayout, device);


  //buildPipeline(vanillaPipeline, device, vanillaShaders,
  //              Buffer<VkVertexInputBindingDescription>& inputBind,
  //              Buffer<VkVertexInputAttributeDescription>& inputAttrib,
  //              VkPipelineLayout pipelineLayout,
  //              VkRenderPass renderPass);


  //execute_end_command_buffer(info);
//execute_queue_command_buffer(info);

  

  logger(0, "moo %d", sizeof(vanillaVS));


}

Renderer::~Renderer()
{
  //vkDestroyPipeline(device, vanillaPipeline, nullptr);
  vkCtx->destroyShader(vanillaPipeline->shader);
  delete vanillaPipeline;
  delete vkCtx;
}

RenderMesh* Renderer::createRenderMesh(Mesh* mesh)
{
  struct VtxNrmTex
  {
    Vec3f vtx;
    Vec3f nrm;
    Vec2f tex;
  };

  auto * renderMesh = new RenderMesh();
  renderMesh->tri_n = mesh->tri_n;
  
  renderMesh->vtxNrmTex = vkCtx->createVertexBuffer(sizeof(VtxNrmTex) * 3 * renderMesh->tri_n);


  VtxNrmTex *mem;
  auto rv = vkMapMemory(vkCtx->device, renderMesh->vtxNrmTex.mem, 0, renderMesh->vtxNrmTex.size, 0, (void **)&mem);
  assert(rv == VK_SUCCESS);

  for (unsigned i = 0; i < 3 * mesh->tri_n; i++) {
    mem[i].vtx = mesh->vtx[mesh->tri[i]];
    mem[i].nrm = Vec3f(0.f);
    mem[i].tex = Vec2f(0.f);
  }
  vkUnmapMemory(vkCtx->device, renderMesh->vtxNrmTex.mem);

  logger(0, "CreateRenderMesh");
  return nullptr;
}

void Renderer::drawRenderMesh(RenderMesh* renderMesh)
{
#if 0

  /* We cannot bind the vertex buffer until we begin a renderpass */
  VkClearValue clear_values[2];
  clear_values[0].color.float32[0] = 0.2f;
  clear_values[0].color.float32[1] = 0.2f;
  clear_values[0].color.float32[2] = 0.2f;
  clear_values[0].color.float32[3] = 0.2f;
  clear_values[1].depthStencil.depth = 1.0f;
  clear_values[1].depthStencil.stencil = 0;

  VkSemaphore imageAcquiredSemaphore;
  VkSemaphoreCreateInfo imageAcquiredSemaphoreCreateInfo;
  imageAcquiredSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  imageAcquiredSemaphoreCreateInfo.pNext = NULL;
  imageAcquiredSemaphoreCreateInfo.flags = 0;

  res = vkCreateSemaphore(info.device, &imageAcquiredSemaphoreCreateInfo, NULL, &imageAcquiredSemaphore);
  assert(res == VK_SUCCESS);

  // Get the index of the next available swapchain image:
  res = vkAcquireNextImageKHR(info.device, info.swap_chain, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE,
                              &info.current_buffer);
  // TODO: Deal with the VK_SUBOPTIMAL_KHR and VK_ERROR_OUT_OF_DATE_KHR
  // return codes
  assert(res == VK_SUCCESS);

  VkRenderPassBeginInfo rp_begin = {};
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = NULL;
  rp_begin.renderPass = info.render_pass;
  rp_begin.framebuffer = info.framebuffers[info.current_buffer];
  rp_begin.renderArea.offset.x = 0;
  rp_begin.renderArea.offset.y = 0;
  rp_begin.renderArea.extent.width = info.width;
  rp_begin.renderArea.extent.height = info.height;
  rp_begin.clearValueCount = 2;
  rp_begin.pClearValues = clear_values;

  vkCmdBeginRenderPass(info.cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindVertexBuffers(info.cmd, 0,             /* Start Binding */
                         1,                       /* Binding Count */
                         &info.vertex_buffer.buf, /* pBuffers */
                         offsets);                /* pOffsets */

  vkCmdEndRenderPass(info.cmd);
  execute_end_command_buffer(info);
  execute_queue_command_buffer(info);
#endif
}

void Renderer::destroyRenderMesh(RenderMesh* renderMesh)
{
  vkDestroyBuffer(vkCtx->device, renderMesh->vtxNrmTex.buffer, NULL);
  vkFreeMemory(vkCtx->device, renderMesh->vtxNrmTex.mem, NULL);
  logger(0, "destroyRenderMesh");
  delete renderMesh;
}

