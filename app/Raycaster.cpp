#include "Raycaster.h"
#include "App.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"
#include "Mesh.h"
#include "../shaders/raytrace.binding.h"

namespace {

  uint32_t raytrace_rgen[] = {
#include "raytrace.rgen.glsl.h"
  };

  uint32_t raytrace_rchit[] = {
#include "raytrace.rchit.glsl.h"
  };

  uint32_t raytrace_rahit[] = {
#include "raytrace.rahit.glsl.h"
  };

  uint32_t raytrace_rmiss[] = {
#include "raytrace.rmiss.glsl.h"
  };

  // Missing VkInstanceNVX structure?
  struct Instance
  {
    float transform[12];
    uint32_t instanceID : 24;
    uint32_t instanceMask : 8;
    uint32_t instanceContributionToHitGroupIndex : 24;
    uint32_t flags : 8;
    uint64_t accelerationStructureHandle; // vkGetAccelerationStructureHandleNVX.
  };

  struct SceneBuffer
  {
    Mat4f Pinv;
    float lx, ly, lz;   // light at top right behind cam
    float ux, uy, uz;   // camera up
    uint32_t rndState;
    uint32_t stationaryFrames;
  };

  struct TriangleData
  {
    float n0x, n0y, n0z;
    float n1x, n1y, n1z;
    float n2x, n2y, n2z;
    float r, g, b;
  };


}

Raycaster::Raycaster(Logger logger, App* app) :
  logger(logger),
  app(app)
{
}

Raycaster::~Raycaster()
{
}

void Raycaster::init()
{
}


VkDescriptorSetLayout Raycaster::buildDescriptorSetLayout()
{
  auto * vCtx = app->vCtx;
  VkDescriptorSetLayoutBinding descSetLayoutBinding[5];
  descSetLayoutBinding[0] = {};
  descSetLayoutBinding[0].binding = BINDING_TOPLEVEL_ACC;
  descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
  descSetLayoutBinding[0].descriptorCount = 1;
  descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

  descSetLayoutBinding[1] = {};
  descSetLayoutBinding[1].binding = BINDING_OUTPUT_IMAGE;
  descSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descSetLayoutBinding[1].descriptorCount = 1;
  descSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

  descSetLayoutBinding[2] = {};
  descSetLayoutBinding[2].binding = BINDING_SCENE_BUF;
  descSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descSetLayoutBinding[2].descriptorCount = 1;
  descSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV;

  descSetLayoutBinding[3] = {};
  descSetLayoutBinding[3].binding = BINDING_INPUT_IMAGE;
  descSetLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descSetLayoutBinding[3].descriptorCount = 1;
  descSetLayoutBinding[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

  descSetLayoutBinding[4] = {};
  descSetLayoutBinding[4].binding = BINDING_TRIANGLE_DATA;
  descSetLayoutBinding[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descSetLayoutBinding[4].descriptorCount = meshData.size32();
  descSetLayoutBinding[4].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;

  VkDescriptorBindingFlagsEXT flags[5] = { 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT };

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT };
  bindingFlags.pBindingFlags = flags;
  bindingFlags.bindingCount = ARRAYSIZE(flags);

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  layoutCreateInfo.pNext = &bindingFlags;
  layoutCreateInfo.bindingCount = ARRAYSIZE(descSetLayoutBinding);
  layoutCreateInfo.pBindings = descSetLayoutBinding;
  CHECK_VULKAN(vkCreateDescriptorSetLayout(vCtx->device, &layoutCreateInfo, nullptr, &layout));
  return layout;
}


void Raycaster::buildPipeline()
{
  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  rtProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV };
  VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
  props2.pNext = &rtProps;
  vkGetPhysicalDeviceProperties2(vCtx->physicalDevice, &props2);

  logger(0, "rtprops.shaderGroupHandleSize=%d", rtProps.shaderGroupHandleSize);
  logger(0, "rtprops.maxRecursionDepth=%d", rtProps.maxRecursionDepth);
  logger(0, "rtprops.maxGeometryCount=%d", rtProps.maxGeometryCount);

  rgenShader = resources->createShader(raytrace_rgen, sizeof(raytrace_rgen));
  chitShader = resources->createShader(raytrace_rchit, sizeof(raytrace_rchit));
  missShader = resources->createShader(raytrace_rmiss, sizeof(raytrace_rmiss));

  VkPipelineShaderStageCreateInfo stages[] = {
    rgenShader.resource->stageCreateInfo,
    chitShader.resource->stageCreateInfo,
    missShader.resource->stageCreateInfo,
  };
  //uint32_t groupNumbers[ARRAYSIZE(stages)] = { 0, 1, 2 };

  VkRayTracingShaderGroupCreateInfoNV groups[] = {
    // group0 -> rgen
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 0, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
    // group1 -> chit
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, VK_SHADER_UNUSED_NV, 1, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV },
    // group2 -> miss
    {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 2, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV }
  };

  pipeline = vCtx->resources->createPipeline();
  auto * pipe = pipeline.resource;


  pipe->descLayout = buildDescriptorSetLayout();

  {
    VkPipelineLayoutCreateInfo pipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &pipe->descLayout;
    pipeLayoutInfo.pushConstantRangeCount = 0;
    pipeLayoutInfo.pPushConstantRanges = NULL;
    CHECK_VULKAN(vkCreatePipelineLayout(vCtx->device, &pipeLayoutInfo, nullptr, &pipe->pipeLayout));
  }

  {

    VkRayTracingPipelineCreateInfoNV info = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV };
    info.pStages = stages;
    info.stageCount = ARRAYSIZE(stages);
    info.pGroups = groups;
    info.groupCount = ARRAYSIZE(groups);
    info.maxRecursionDepth = 10;
    info.layout = pipe->pipeLayout;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = 0;
    CHECK_VULKAN(vCtx->vkCreateRayTracingPipelinesNV(vCtx->device, nullptr /*vCtx->pipelineCache*/, 1, &info, nullptr, &pipe->pipe));
  }

  //CHECK_VULKAN(vCtx->vkCompileDeferredNVX(vCtx->device, pipe->pipe, 0));
  { // shader binding table
    uint32_t groupCount = 3;
    uint32_t tableSize = groupCount * rtProps.shaderGroupHandleSize;
    bindingTable = vCtx->resources->createBuffer(tableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    CHECK_VULKAN(vCtx->vkGetRayTracingShaderGroupHandlesNV(vCtx->device, pipe->pipe, 0, groupCount, tableSize, bindingTable.resource->hostPtr));
  }

  
}


bool Raycaster::updateMeshData(VkDeviceSize& scratchSize, MeshData& meshData,  const Mesh* mesh)
{
  auto * vCtx = app->vCtx;
  auto * res = vCtx->resources;

  if (meshData.geometryGeneration == mesh->geometryGeneration && meshData.colorGeneration == mesh->colorGeneration) return false;
  meshData.geometryGeneration = mesh->geometryGeneration;
  meshData.colorGeneration = mesh->colorGeneration;

  logger(0, "Updating Raytracer.MeshData item.");

  meshData.vertexCount = mesh->vtxCount;
  meshData.triangleCount = mesh->triCount;
  meshData.vertices = res->createBuffer(sizeof(Vec3f)*meshData.vertexCount,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vCtx->resources->copyHostMemToBuffer(meshData.vertices, mesh->vtx, sizeof(Vec3f)*meshData.vertexCount);
  meshData.indices = vCtx->resources->createBuffer(sizeof(uint32_t) * 3 * meshData.triangleCount,
                                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vCtx->resources->copyHostMemToBuffer(meshData.indices, mesh->triVtxIx, sizeof(uint32_t) * 3 * meshData.triangleCount);

  meshData.triangleData = res->createBuffer(sizeof(TriangleData)*meshData.triangleCount,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  {
    auto * mem = (TriangleData*)meshData.triangleData.resource->hostPtr;
    if (mesh->nrmCount) {
      for (uint32_t t = 0; t < meshData.triangleCount; t++) {
        auto & data = mem[t];
        auto n0 = normalize(mesh->nrm[mesh->triNrmIx[3 * t + 0]]);
        auto n1 = normalize(mesh->nrm[mesh->triNrmIx[3 * t + 1]]);
        auto n2 = normalize(mesh->nrm[mesh->triNrmIx[3 * t + 2]]);
        data.n0x = n0.x;// uint8_t(127.f*n0.x + 127.f);
        data.n0y = n0.y;// uint8_t(127.f*n0.y + 127.f);
        data.n0z = n0.z;// uint8_t(127.f*n0.z + 127.f);
        data.n1x = n1.x;// uint8_t(127.f*n1.x + 127.f);
        data.n1y = n1.y;// uint8_t(127.f*n1.y + 127.f);
        data.n1z = n1.z;// uint8_t(127.f*n1.z + 127.f);
        data.n2x = n2.x;// (127.f*n2.x + 127.f);
        data.n2y = n2.y;// uint8_t(127.f*n2.y + 127.f);
        data.n2z = n2.z;// uint8_t(127.f*n2.z + 127.f);
        auto color = t;// mesh->currentColor[t];
        data.r = (1.f / 255.f)*((color >> 16) & 0xff);
        data.g = (1.f / 255.f)*((color >> 8) & 0xff);
        data.b = (1.f / 255.f)*(color & 0xff);
      }
    }
    else {
      for (uint32_t t = 0; t < meshData.triangleCount; t++) {
        Vec3f p[3];
        for (unsigned k = 0; k < 3; k++) p[k] = mesh->vtx[mesh->triVtxIx[3 * t + k]];
        auto n = normalize(cross(p[1] - p[0], p[2] - p[0]));
        auto & data = mem[t];
        data.n0x = n.x;// uint8_t(127.f*n0.x + 127.f);
        data.n0y = n.y;// uint8_t(127.f*n0.y + 127.f);
        data.n0z = n.z;// uint8_t(127.f*n0.z + 127.f);
        data.n1x = n.x;// uint8_t(127.f*n1.x + 127.f);
        data.n1y = n.y;// uint8_t(127.f*n1.y + 127.f);
        data.n1z = n.z;// uint8_t(127.f*n1.z + 127.f);
        data.n2x = n.x;// (127.f*n2.x + 127.f);
        data.n2y = n.y;// uint8_t(127.f*n2.y + 127.f);
        data.n2z = n.z;// uint8_t(127.f*n2.z + 127.f);
        auto color = t;// mesh->currentColor[t];
        data.r = (1.f / 255.f)*((color >> 16) & 0xff);
        data.g = (1.f / 255.f)*((color >> 8) & 0xff);
        data.b = (1.f / 255.f)*(color & 0xff);
      }

    }
  }

  auto & geometry = meshData.geometry;
  geometry = { VK_STRUCTURE_TYPE_GEOMETRY_NV };

  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
  geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
  geometry.geometry.triangles.pNext = nullptr;
  geometry.geometry.triangles.vertexData = meshData.vertices.resource->buffer;
  geometry.geometry.triangles.vertexOffset = 0;
  geometry.geometry.triangles.vertexCount = meshData.vertexCount;
  geometry.geometry.triangles.vertexStride = sizeof(Vec3f);
  geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  geometry.geometry.triangles.indexData = meshData.indices.resource->buffer; //idxBuf.resource->buffer;
  geometry.geometry.triangles.indexOffset = 0;
  geometry.geometry.triangles.indexCount = 3 * meshData.triangleCount;
  geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
  geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
  geometry.geometry.triangles.transformOffset = 0;
  geometry.geometry.aabbs = { };
  geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
  geometry.flags = 0;

  meshData.acc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV, 1, &geometry, 0);
  if (scratchSize < meshData.acc.resource->scratchReqs.size) scratchSize = meshData.acc.resource->scratchReqs.size;

  meshData.rebuild = true;
  return true;
}


void Raycaster::update(Vector<Mesh*>& meshes)
{
  auto * vCtx = app->vCtx;
  auto & frame = vCtx->frameManager->frame();

  VkDeviceSize scratchSize = 0;


  bool change = false;

  newMeshData.clear();
  newMeshData.reserve(meshes.size());
  for (auto & mesh : meshes) {
    bool found = false;
    for (auto & item : meshData) {
      if (item.src == mesh) {
        newMeshData.pushBack(item);
        found = true;
        break;
      }
    }
    if (!found) {
      MeshData md{};
      md.src = mesh;
      newMeshData.pushBack(md);
      logger(0, "Created new MeshData item.");
    }
    if (updateMeshData(scratchSize, newMeshData.back(), mesh)) {
      change = true;
    }
  }
  meshData.swap(newMeshData);
  if (change) {
    if (meshData.empty()) {
      topAcc = AccelerationStructureHandle();
      return;
    }
    auto geometryCount = meshData.size32();

    Vector<VkGeometryNV> geometries(geometryCount);
    Vector<Instance> instances(geometryCount);

    geometries.resize(geometryCount);
    for (uint32_t g = 0; g < geometryCount; g++) {
      geometries[g] = meshData[g].geometry;
      auto & instance = instances[g];
      for (unsigned j = 0; j < 3; j++) {
        for (unsigned i = 0; i < 4; i++) {
          instance.transform[4 * j + i] = i == j ? 1.f : 0.f;
        }
      }
      instance.instanceID = 0;
      instance.instanceMask = 0xff;
      instance.instanceContributionToHitGroupIndex = 0;
      instance.flags = 0;// VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
      instance.accelerationStructureHandle = 0;
      CHECK_VULKAN(vCtx->vkGetAccelerationStructureHandleNV(vCtx->device, meshData[g].acc.resource->acc, sizeof(uint64_t), &instance.accelerationStructureHandle));
    }
    auto instanceBuffer = vCtx->resources->createBuffer(sizeof(Instance)*geometryCount, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vCtx->resources->copyHostMemToBuffer(instanceBuffer, instances.data(), sizeof(Instance)*geometryCount);

    topAcc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV, 0, nullptr, uint32_t(instances.size()));
    if (scratchSize < topAcc.resource->scratchReqs.size) scratchSize = topAcc.resource->scratchReqs.size;

    auto scratchBuffer = vCtx->resources->createBuffer(scratchSize, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

    auto cmdBuf = vCtx->frameManager->createPrimaryCommandBuffer();
    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    for (unsigned g = 0; g < geometryCount; g++) {
      if (meshData[g].rebuild) {
        meshData[g].rebuild = false;
        {
          VkAccelerationStructureInfoNV asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
          asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
          asInfo.flags = 0;
          asInfo.instanceCount = 0;
          asInfo.pGeometries = &geometries[g];
          asInfo.geometryCount = 1;// (uint32_t)geometries.size();
          vCtx->vkCmdBuildAccelerationStructureNV(cmdBuf, &asInfo, VK_NULL_HANDLE, 0, VK_FALSE, meshData[g].acc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);
        }
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
      }
    }

    {
      VkAccelerationStructureInfoNV asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
      asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
      asInfo.flags = 0;
      asInfo.instanceCount = geometryCount;
      asInfo.geometryCount = 0;

      vCtx->vkCmdBuildAccelerationStructureNV(cmdBuf, &asInfo, instanceBuffer.resource->buffer, 0, VK_FALSE, topAcc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);
    }
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vCtx->queue);
    //vkDeviceWaitIdle(vCtx->device);

    buildPipeline();
  }

}


void Raycaster::resize(const Vec4f& viewport)
{
  auto newW = uint32_t(viewport.z < 1 ? 1.f : viewport.z);
  auto newH = uint32_t(viewport.w < 1 ? 1.f : viewport.w);
  if (newW == w && newH == h) return;
  w = newW;
  h = newH;
  stationaryFrames = 0;
  logger(0, "Resized to %dx%d", w, h);

  auto * vCtx = app->vCtx;
  auto * resources = vCtx->resources;

  for (unsigned i = 0; i < 2; i++) {
    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent = { w, h, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    offscreenImage[i] = resources->createImage(imageInfo);

    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    offscreenView[i] = resources->createImageView(offscreenImage[i], viewInfo);
  }
}

VkDescriptorSet Raycaster::setupDescriptorSet(const Mat3f& Ninv, const Mat4f& Pinv, VkImageView outImageView, VkImageView inImageView)
{
  auto * vCtx = app->vCtx;

  VkDescriptorSet set = VK_NULL_HANDLE;

  VkDescriptorBufferInfo sceneBufferInfo;

  auto l = normalize(mul(Ninv, Vec3f(1, 1, 0.2f)));
  auto u = normalize(mul(Ninv, Vec3f(0, 1, 0)));

  auto* sceneBuffer = (SceneBuffer*)vCtx->frameManager->allocUniformStore(sceneBufferInfo, sizeof(SceneBuffer));
  sceneBuffer->Pinv = Pinv;
  sceneBuffer->lx = l.x;
  sceneBuffer->ly = l.y;
  sceneBuffer->lz = l.z;
  sceneBuffer->ux = u.x;
  sceneBuffer->uy = u.y;
  sceneBuffer->uz = u.z;
  sceneBuffer->rndState = rndState;
  sceneBuffer->stationaryFrames = stationaryFrames;

  //VkDescriptorSet set = vCtx->frameManager->allocDescriptorSet(pipeline);

  uint32_t varDescCount = meshData.size32();

  set = vCtx->frameManager->allocVariableDescriptorSet(&varDescCount, 1, pipeline);

  Vector<VkWriteDescriptorSet> writes(5);
  for (auto & write : writes) {
    write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet = set;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
  }

  // ----
  VkWriteDescriptorSetAccelerationStructureNV descriptorAccelerationStructureInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV };
  descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
  descriptorAccelerationStructureInfo.pAccelerationStructures = &topAcc.resource->acc;

  writes[0].pNext = &descriptorAccelerationStructureInfo;
  writes[0].dstBinding = BINDING_TOPLEVEL_ACC;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

  // ----
  VkDescriptorImageInfo outImageInfo{};
  outImageInfo.imageView = outImageView;
  outImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  writes[1].dstBinding = BINDING_OUTPUT_IMAGE;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[1].pImageInfo = &outImageInfo;

  // ----
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[2].pBufferInfo = &sceneBufferInfo;
  writes[2].dstBinding = BINDING_SCENE_BUF;

  // ----
  VkDescriptorImageInfo inImageInfo{};
  inImageInfo.imageView = inImageView;
  inImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  writes[3].dstBinding = BINDING_INPUT_IMAGE;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writes[3].pImageInfo = &inImageInfo;

  // ----
  Vector<VkDescriptorBufferInfo> bufInfo(meshData.size32());
  for (uint32_t g = 0; g < meshData.size32(); g++) {
    bufInfo[g].buffer = meshData[g].triangleData.resource->buffer;
    bufInfo[g].offset = 0;
    bufInfo[g].range = VK_WHOLE_SIZE;
  }
  writes[4].dstArrayElement = 0;
  writes[4].descriptorCount = bufInfo.size32();
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writes[4].pBufferInfo = bufInfo.data();
  writes[4].dstBinding = BINDING_TRIANGLE_DATA;

  vkUpdateDescriptorSets(vCtx->device, writes.size32(), writes.data(), 0, nullptr);

  return set;
}


void Raycaster::draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat3f& Ninv, const Mat4f& Pinv)
{
  auto * vCtx = app->vCtx;
  if (!topAcc) return;

  resize(viewport);

  for (uint32_t i = 0; i < 16; i++) {
    if (1e-7 < std::abs(Pinv.data[i] - prevPinv.data[i])) {
      stationaryFrames = 0;
      break;
    }
  }
  prevPinv = Pinv;

  rndState = 1664525 * rndState + 1013904223;

  auto prevOffscreenImage = offscreenImage[offscreenIndex].resource->image;
  auto prevOffscreenView = offscreenView[offscreenIndex].resource->view;
  offscreenIndex = (offscreenIndex + 1) & 1;
  auto currOffscreenImage = offscreenImage[offscreenIndex].resource->image;
  auto currOffscreenView = offscreenView[offscreenIndex].resource->view;
  auto onscreenImage = vCtx->frameManager->backBufferImages[vCtx->frameManager->swapChainIndex];


  VkDescriptorSet set = setupDescriptorSet(Ninv, Pinv, currOffscreenView, prevOffscreenView);

  {
    VkImageMemoryBarrier imageMemoryBarriers[2];
    imageMemoryBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }; // Prep prev image for reading
    imageMemoryBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarriers[0].dstAccessMask = 0;
    imageMemoryBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].image = prevOffscreenImage;
    imageMemoryBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    imageMemoryBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }; // Prep for shader storage
    imageMemoryBarriers[1].srcAccessMask = 0;
    imageMemoryBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].image = currOffscreenImage;
    imageMemoryBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 2, imageMemoryBarriers);
  }
  {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline.resource->pipeLayout, 0, 1, &set, 0, 0);
    vCtx->vkCmdTraceRaysNV(cmdBuf,
                           bindingTable.resource->buffer, 0,      // raygen
                           bindingTable.resource->buffer, 2 * rtProps.shaderGroupHandleSize, rtProps.shaderGroupHandleSize,   // miss
                           bindingTable.resource->buffer, 1 * rtProps.shaderGroupHandleSize, rtProps.shaderGroupHandleSize,   // hit
                           VK_NULL_HANDLE, 0, 0,
                           w, h, 1);
  }
  { // Prep for blitting
    VkImageMemoryBarrier imageMemoryBarriers[2];
    imageMemoryBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    imageMemoryBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    imageMemoryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[0].image = currOffscreenImage;
    imageMemoryBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    imageMemoryBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarriers[1].srcAccessMask = 0;// srcAccessMask;
    imageMemoryBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].image = onscreenImage;
    imageMemoryBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 2, imageMemoryBarriers);
  }
  { // blit
#if 1
    VkImageBlit blit;
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[0] = { 0, 0, 0 };
    blit.srcOffsets[1] = { int32_t(w), int32_t(h), 1 };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[0] = { int32_t(viewport.x), int32_t(viewport.y), 0 };
    blit.dstOffsets[1] = { int32_t(viewport.x) + int32_t(w), int32_t(viewport.y) + int32_t(h), 1 };
    vkCmdBlitImage(cmdBuf,
                   currOffscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   onscreenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_NEAREST);
#else
    VkImageCopy imageCopy{};
    imageCopy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    imageCopy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    imageCopy.dstOffset = { int32_t(viewport.x), int32_t(viewport.y), 0 };
    imageCopy.extent = { w, h, 1 };
    vkCmdCopyImage(cmdBuf,
                   currOffscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   onscreenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
#endif
  }
  { // Prop for presentiation
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = onscreenImage;
    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  stationaryFrames++;
}
