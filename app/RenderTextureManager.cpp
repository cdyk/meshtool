#include "RenderTextureManager.h"
#include "VulkanContext.h"


RenderTextureManager::RenderTextureManager(VulkanContext* vCtx) :
  ResourceManager(nullptr, nullptr),
  vCtx(vCtx)
{

}

RenderTextureHandle RenderTextureManager::loadTexture(TextureSource source)
{
  uint32_t texW = 500;
  uint32_t texH = 500;

  auto handle = createResource();
  auto * res = handle.resource;

  res->stagingBuffer = vCtx->resources->createStagingBuffer(texW*texH * 4);
  {
    auto * mem = (uint8_t*)res->stagingBuffer.resource->hostPtr;
    switch (source) {

    case TextureSource::Checker:
      for (unsigned j = 0; j < texH; j++) {
        for (unsigned i = 0; i < texW; i++) {
          auto g = ((i / (texW / 10) + j / (texH / 10))) & 1 ? 0 : 20;
          auto f = ((i / (texW / 2) + j / (texH / 2))) & 1 ? 150 : 235;
          mem[4 * (texW*j + i) + 0] = f + g;
          mem[4 * (texW*j + i) + 1] = f + g;
          mem[4 * (texW*j + i) + 2] = f + g;
          mem[4 * (texW*j + i) + 3] = 255;
        }
      }
      break;

    case TextureSource::ColorGradient:
      for (unsigned j = 0; j < texH; j++) {
        for (unsigned i = 0; i < texW; i++) {
          mem[4 * (texW*j + i) + 0] = (255 * (i)) / (texW);
          mem[4 * (texW*j + i) + 1] = (255 * (j)) / (texH);
          mem[4 * (texW*j + i) + 2] = 0;
          mem[4 * (texW*j + i) + 3] = 255;
        }
      }
      break;

    default:
      assert(false && "Invalid texture source");
    }
  }
  VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
  info.imageType = VK_IMAGE_TYPE_2D;
  info.extent.width = static_cast<uint32_t>(texW);
  info.extent.height = static_cast<uint32_t>(texH);
  info.extent.depth = 1;
  info.mipLevels = 1;
  info.arrayLayers = 1;
  info.format = VK_FORMAT_R8G8B8A8_UNORM;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.flags = 0;

  VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
  viewInfo.format = VK_FORMAT_UNDEFINED;
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
  viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.flags = 0;

  res->image = vCtx->resources->createImage(info);
  res->view = vCtx->resources->createImageView(res->image, viewInfo);
  res->state = RenderResourceState::Loading;

  // FIXME: should be stashed on transfer queue and polled at startframe.
  vCtx->frameManager->transitionImageLayout(res->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vCtx->frameManager->copyBufferToImage(res->image, res->stagingBuffer, texW, texH);
  vCtx->frameManager->transitionImageLayout(res->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  res->state = RenderResourceState::Ready;

  return handle;
}
