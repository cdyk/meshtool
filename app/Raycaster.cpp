#include "Raycaster.h"
#include "VulkanManager.h"
#include "VulkanContext.h"

namespace {

  uint32_t raytrace_rgen[] = {
#include "raytrace.rgen.glsl.h"
  };
}

Raycaster::Raycaster(Logger logger, VulkanManager* vulkanManager)
  : logger(logger),
    vulkanManager(vulkanManager)
{
}

Raycaster::~Raycaster()
{
}

void Raycaster::init()
{
  auto * vCtx = vulkanManager->vCtx;

  Vector<ShaderInputSpec> stages(1);
  stages[0] = { raytrace_rgen, sizeof(raytrace_rgen), VK_SHADER_STAGE_RAYGEN_BIT_NVX };

  //ShaderInputSpec shaders[] = {
  //  ,
  //  { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_ANY_HIT_BIT_NVX },
  //  { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX },
  //  { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_MISS_BIT_NVX },
  //  { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_INTERSECTION_BIT_NVX },
  //  { vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_CALLABLE_BIT_NVX },
  //}

  shader = vCtx->resources->createShader(stages);

  pipeline = vCtx->resources->createPipeline();
  auto * pipe = pipeline.resource;

  //VkPipeline pipe = VK_NULL_HANDLE;
  //VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
  //VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
  //RenderPassHandle pass;


  VkDescriptorSetLayoutBinding descSetLayoutBinding[2];
  descSetLayoutBinding[0] = {};
  descSetLayoutBinding[0].binding = 0;
  descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descSetLayoutBinding[0].descriptorCount = 1;
  descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
  descSetLayoutBinding[1].binding = 1;
  descSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
  descSetLayoutBinding[1].descriptorCount = 1;
  descSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;

  VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  layoutCreateInfo.bindingCount = 2;
  layoutCreateInfo.pBindings = descSetLayoutBinding;

  CHECK_VULKAN(vkCreateDescriptorSetLayout(vCtx->device, &layoutCreateInfo, nullptr, &pipe->descLayout));

  VkPipelineLayoutCreateInfo pipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  pipeLayoutInfo.setLayoutCount = 1;
  pipeLayoutInfo.pSetLayouts = &pipe->descLayout;
  pipeLayoutInfo.pushConstantRangeCount = 0;
  pipeLayoutInfo.pPushConstantRanges = NULL;
  CHECK_VULKAN(vkCreatePipelineLayout(vCtx->device, &pipeLayoutInfo, nullptr, &pipe->pipeLayout));

  Vector<uint32_t> groupNumbers(stages.size());

  VkRaytracingPipelineCreateInfoNVX info = { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };
  info.flags = VK_PIPELINE_CREATE_DEFER_COMPILE_BIT_NVX;
  info.pStages = shader.resource->stageCreateInfo.data();
  info.stageCount = shader.resource->stageCreateInfo.size32();
  info.pGroupNumbers = groupNumbers.data();
  info.maxRecursionDepth = 5;
  info.layout = pipe->pipeLayout;
  info.basePipelineIndex = -1;
  CHECK_VULKAN(vCtx->vkCreateRaytracingPipelinesNVX(vCtx->device, vCtx->pipelineCache, 1, &info, nullptr, &pipe->pipe));

}