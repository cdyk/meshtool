#include <cassert>
#include "Common.h"
#include "Renderer.h"


namespace {

  uint32_t vanillaVS[]
#include "vanillaVS.glsl.h"
    ;

  uint32_t vanillaPS[]
#include "vanillaPS.glsl.h"
    ;

  struct ShaderInputSpec {
    uint32_t* spv;
    size_t siz;
    VkShaderStageFlagBits stage;
  };

  void buildShaders(Buffer<VkPipelineShaderStageCreateInfo>& stageCreateInfo, VkDevice device, ShaderInputSpec* spec, unsigned N)
  {
    stageCreateInfo.accommodate(N);
    for (unsigned i = 0; i < 2; i++) {
      stageCreateInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stageCreateInfo[i].pNext = nullptr;
      stageCreateInfo[i].pSpecializationInfo = nullptr;
      stageCreateInfo[i].flags = 0;
      stageCreateInfo[i].pName = "main";
      stageCreateInfo[i].stage = spec[i].stage;

      VkShaderModuleCreateInfo moduleCreateInfo;
      moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      moduleCreateInfo.pNext = nullptr;
      moduleCreateInfo.flags = 0;
      moduleCreateInfo.codeSize = spec[i].siz;
      moduleCreateInfo.pCode = spec[i].spv;
      auto rv = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &stageCreateInfo[i].module);
      assert(rv == VK_SUCCESS);
    }

  }


}


Renderer::Renderer(Logger logger, VkDevice device) :
  device(device)
{
  ShaderInputSpec stages[] = {
    {vanillaVS, sizeof(vanillaVS), VK_SHADER_STAGE_VERTEX_BIT},
    {vanillaPS, sizeof(vanillaPS), VK_SHADER_STAGE_FRAGMENT_BIT}
  };

  buildShaders(vanillaShader, device,  stages, 2);

  



  logger(0, "moo %d", sizeof(vanillaVS));


}

Renderer::~Renderer()
{
  for (size_t i = 0; i < vanillaShader.getCount(); i++) {
    vkDestroyShaderModule(device, vanillaShader[i].module, nullptr);
  }
}
