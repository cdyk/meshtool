#include "Raycaster.h"
#include "VulkanManager.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"

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


  rtProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAYTRACING_PROPERTIES_NVX };
  VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
  props2.pNext = &rtProps;
  vkGetPhysicalDeviceProperties2(vCtx->physicalDevice, &props2);

  logger(0, "rtprops.shaderHeaderSize=%d", rtProps.shaderHeaderSize);
  logger(0, "rtprops.maxRecursionDepth=%d", rtProps.maxRecursionDepth);
  logger(0, "rtprops.maxGeometryCount=%d", rtProps.maxGeometryCount);


  Vector<ShaderInputSpec> stages(1);
  stages[0] = { raytrace_rgen, sizeof(raytrace_rgen), VK_SHADER_STAGE_RAYGEN_BIT_NVX };
  //stages[1] = { raytrace_rmiss, sizeof(raytrace_rmiss), VK_SHADER_STAGE_MISS_BIT_NVX };
  //stages[2] = { raytrace_rchit, sizeof(raytrace_rchit), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX };
  //stages[3] = { raytrace_rahit, sizeof(raytrace_rahit), VK_SHADER_STAGE_ANY_HIT_BIT_NVX };

  Vector<uint32_t> groupNumbers(stages.size());
  groupNumbers[0] = 0;
  //groupNumbers[1] = 1;
  //groupNumbers[2] = 2;
  //groupNumbers[3] = 2;

  shader = vCtx->resources->createShader(stages);
  assert(shader.resource->stageCreateInfo.size() == stages.size());

  pipeline = vCtx->resources->createPipeline();
  auto * pipe = pipeline.resource;


  {
    VkDescriptorSetLayoutBinding descSetLayoutBinding[3];
    descSetLayoutBinding[0] = {};
    descSetLayoutBinding[0].binding = 0;
    descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    descSetLayoutBinding[0].descriptorCount = 1;
    descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    descSetLayoutBinding[1] = {};
    descSetLayoutBinding[1].binding = 1;
    descSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descSetLayoutBinding[1].descriptorCount = 1;
    descSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    descSetLayoutBinding[2] = {};
    descSetLayoutBinding[2].binding = 2;
    descSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetLayoutBinding[2].descriptorCount = 1;
    descSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutCreateInfo.bindingCount = sizeof(descSetLayoutBinding) / sizeof(descSetLayoutBinding[0]);
    layoutCreateInfo.pBindings = descSetLayoutBinding;
    CHECK_VULKAN(vkCreateDescriptorSetLayout(vCtx->device, &layoutCreateInfo, nullptr, &pipe->descLayout));
  }

  {
    VkPipelineLayoutCreateInfo pipeLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &pipe->descLayout;
    pipeLayoutInfo.pushConstantRangeCount = 0;
    pipeLayoutInfo.pPushConstantRanges = NULL;
    CHECK_VULKAN(vkCreatePipelineLayout(vCtx->device, &pipeLayoutInfo, nullptr, &pipe->pipeLayout));
  }

  {

    VkRaytracingPipelineCreateInfoNVX info = { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };
    info.pStages = shader.resource->stageCreateInfo.data();
    info.stageCount = shader.resource->stageCreateInfo.size32();
    info.pGroupNumbers = groupNumbers.data();
    info.maxRecursionDepth = 1;
    info.layout = pipe->pipeLayout;
    CHECK_VULKAN(vCtx->vkCreateRaytracingPipelinesNVX(vCtx->device, nullptr /*vCtx->pipelineCache*/, 1, &info, nullptr, &pipe->pipe));
  }

  //CHECK_VULKAN(vCtx->vkCompileDeferredNVX(vCtx->device, pipe->pipe, 0));

  /*
  size_t bindingTableSize = 3 * rtProps.shaderHeaderSize;
  auto bindingTableStage = vCtx->resources->createStagingBuffer(bindingTableSize);
  {
    char* ptr = nullptr;
    MappedBufferBase map((void**)&ptr, vCtx, bindingTableStage);
    // This one grabs handles by the group defined above
    // First, handle of ray generation shader, followed by contents of shaderRecordNVX.
    CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 0, 1, bindingTableSize, ptr + 0 * rtProps.shaderHeaderSize));
    CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 1, 1, bindingTableSize, ptr + 1 * rtProps.shaderHeaderSize));
    CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 2, 1, bindingTableSize, ptr + 2 * rtProps.shaderHeaderSize));
    // instanceShaderBindingTableRecordOffset stored in each instance of top-level acc struc
    // geometry index is the geometry within the instance.
    // instanceShaderBindingTableRecordOffset + hitProgramShaderBindingTableBaseIndex + geometryIndex � sbtRecordStride + sbtRecordOffset
  }

  bindingTable = vCtx->resources->createBuffer(bindingTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vCtx->frameManager->copyBuffer(bindingTable, bindingTableStage, bindingTableSize);
  */

  renames.resize(5);
  VkDescriptorPoolSize poolSizes[] = {
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, renames.size32()},
      { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, renames.size32() },
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  renames.size32() }
  };

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.pNext = nullptr;
  descriptorPoolCreateInfo.flags = 0;
  descriptorPoolCreateInfo.maxSets = renames.size32();
  descriptorPoolCreateInfo.poolSizeCount = ARRAYSIZE(poolSizes);
  descriptorPoolCreateInfo.pPoolSizes = poolSizes;
  CHECK_VULKAN(vkCreateDescriptorPool(vCtx->device, &descriptorPoolCreateInfo, nullptr, &descPool));

  for (auto & rename : renames) {

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = nullptr;
    descriptorSetAllocateInfo.descriptorPool = descPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &pipe->descLayout;
    CHECK_VULKAN(vkAllocateDescriptorSets(vCtx->device, &descriptorSetAllocateInfo, &rename.descSet));
  }
}

void Raycaster::update(Vector<RenderMeshHandle>& meshes)
{
  if (!first) return;
  first = false;

  auto * vCtx = vulkanManager->vCtx;
  auto & frame = vCtx->frameManager->frame();

  std::vector<VkGeometryNVX> geometries;
  VkDeviceSize scratchSize = 0;

  Vec3f vertices[3] = {
      Vec3f( -0.5f, -0.5f, 0.0f ),
      Vec3f(+0.0f, +0.5f, 0.0f ),
      Vec3f(+0.5f, -0.5f, 0.0f )
  };
  auto vtxBuf = vCtx->resources->createBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vCtx->resources->copyHostMemToBuffer(vtxBuf, vertices, sizeof(vertices));

  uint16_t indices[] = { 0, 1, 2 };
  auto idxBuf = vCtx->resources->createBuffer(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vCtx->resources->copyHostMemToBuffer(idxBuf, indices, sizeof(indices));

  geometries.push_back({ VK_STRUCTURE_TYPE_GEOMETRY_NVX });
  auto & geometry = geometries.back();
  //VkGeometryNVX geometry;
  geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NVX;
  geometry.pNext = nullptr;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
  geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
  geometry.geometry.triangles.pNext = nullptr;
  geometry.geometry.triangles.vertexData = vtxBuf.resource->buffer;
  geometry.geometry.triangles.vertexOffset = 0;
  geometry.geometry.triangles.vertexCount = ARRAYSIZE(vertices);
  geometry.geometry.triangles.vertexStride = sizeof(Vec3f);
  geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  geometry.geometry.triangles.indexData = idxBuf.resource->buffer;
  geometry.geometry.triangles.indexOffset = 0;
  geometry.geometry.triangles.indexCount = ARRAYSIZE(indices);
  geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
  geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
  geometry.geometry.triangles.transformOffset = 0;
  geometry.geometry.aabbs = { };
  geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX;
  geometry.flags = 0;
  acc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX, 1, &geometry, 0);
  if (scratchSize < acc.resource->scratchReqs.size) scratchSize = acc.resource->scratchReqs.size;


  std::vector<Instance> instances(1);
  auto & instance = instances[0];
  for (unsigned j = 0; j < 3; j++) {
    for (unsigned i = 0; i < 4; i++) {
      instance.transform[4 * j + i] = i == j ? 1.f : 0.f;
    }
  }
  instance.instanceID = 0;
  instance.instanceMask = 0xff;
  instance.instanceContributionToHitGroupIndex = 0;
  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
  instance.accelerationStructureHandle = 0;
  CHECK_VULKAN(vCtx->vkGetAccelerationStructureHandleNVX(vCtx->device, acc.resource->acc, sizeof(uint64_t), &instance.accelerationStructureHandle));

  auto instanceBuffer = vCtx->resources->createBuffer(sizeof(Instance), VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  vCtx->resources->copyHostMemToBuffer(instanceBuffer, instances.data(), sizeof(Instance)*instances.size());

  topAcc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX, 0, nullptr, uint32_t(instances.size()));
  if (scratchSize < topAcc.resource->scratchReqs.size) scratchSize = topAcc.resource->scratchReqs.size;

  auto scratchBuffer = vCtx->resources->createBuffer(scratchSize, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);



  auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(frame.commandPool).resource->cmdBuf;

  VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmdBuf, &beginInfo);


  VkMemoryBarrier memoryBarrier;
  memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  memoryBarrier.pNext = nullptr;
  memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
  memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;


  vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                           0, VK_NULL_HANDLE, 0,
                                           (uint32_t)geometries.size(), &geometries[0],
                                           0, VK_FALSE, acc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);

  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

  vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                           1, instanceBuffer.resource->buffer, 0,
                                           0, nullptr,
                                           0, VK_FALSE, topAcc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);

  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

  vkEndCommandBuffer(cmdBuf);

  VkSubmitInfo submitInfo;
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pNext = nullptr;
  submitInfo.waitSemaphoreCount = 0;
  submitInfo.pWaitSemaphores = nullptr;
  submitInfo.pWaitDstStageMask = nullptr;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdBuf;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores = nullptr;

  vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(vCtx->queue);
  vkDeviceWaitIdle(vCtx->device);

  for (auto & rename : renames) {
    VkDescriptorAccelerationStructureInfoNVX descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topAcc.resource->acc;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
    accelerationStructureWrite.dstSet = rename.descSet;
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;
    vkUpdateDescriptorSets(vCtx->device, (uint32_t)1, &accelerationStructureWrite, 0, nullptr);
  }

  //vkQueueSubmit(_queuesInfo.Graphics.Queue, 1, &submitInfo, VK_NULL_HANDLE);
  //vkQueueWaitIdle(_queuesInfo.Graphics.Queue);
  //vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);


# if 0

  bool change = false;

  newMeshData.clear();
  newMeshData.reserve(meshes.size());
  for (auto & renderMesh : meshes) {
    bool found = false;
    for (auto & item : meshData) {
      if (item.src == renderMesh) {
        newMeshData.pushBack(item);
        found = true;
        break;
      }
    }
    if (!found) {
      MeshData md{};
      md.src = renderMesh;
      newMeshData.pushBack(md);
      logger(0, "Created new MeshData item.");
    }
    auto &md = newMeshData.back();
    if (md.meshGen != renderMesh.resource->generation) {
      change = true;
      logger(0, "Updating MeshData item.");
      auto * rm = renderMesh.resource;
      md.meshGen = rm->generation;

      VkGeometryNVX geometry{ VK_STRUCTURE_TYPE_GEOMETRY_NVX };
      geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
      geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
      geometry.geometry.triangles.vertexData = rm->vtx.resource->buffer;
      geometry.geometry.triangles.vertexOffset = 0;
      geometry.geometry.triangles.vertexCount = 3 * rm->tri_n;
      geometry.geometry.triangles.vertexStride = 3 * sizeof(float);
      geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
      geometry.geometry.triangles.indexData = VK_NULL_HANDLE;
      geometry.geometry.triangles.transformData = VK_NULL_HANDLE;
      geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX;

      // VK_GEOMETRY_OPAQUE_BIT_NVX
      // VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_NVX
      geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NVX;

      VkAccelerationStructureCreateInfoNVX info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
      info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX;
      info.flags = 0;// VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX;
      info.instanceCount = 1;
      info.geometryCount = 1;
      info.pGeometries = &geometry;

      md.acc = vCtx->resources->createAccelerationStructure();
      CHECK_VULKAN(vCtx->vkCreateAccelerationStructureNVX(vCtx->device, &info, nullptr, &md.acc.resource->acc));



      VkAccelerationStructureMemoryRequirementsInfoNVX memReqInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
      memReqInfo.accelerationStructure = md.acc.resource->acc;

      VkMemoryRequirements2KHR strucMemReq;
      vCtx->vkGetAccelerationStructureMemoryRequirementsNVX(vCtx->device, &memReqInfo, &strucMemReq);

      VkMemoryAllocateInfo strucAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
      strucAllocInfo.memoryTypeIndex = 0;
      strucAllocInfo.allocationSize = strucMemReq.memoryRequirements.size;
      CHECK_BOOL(vCtx->resources->getMemoryTypeIndex(strucAllocInfo.memoryTypeIndex, strucMemReq.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
      CHECK_VULKAN(vkAllocateMemory(vCtx->device, &strucAllocInfo, NULL, &md.acc.resource->structureMem));

      VkBindAccelerationStructureMemoryInfoNVX strucBindInfo{ VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX };
      strucBindInfo.accelerationStructure = md.acc.resource->acc;
      strucBindInfo.memory = md.acc.resource->structureMem;     // What are device indices?
      CHECK_VULKAN(vCtx->vkBindAccelerationStructureMemoryNVX(vCtx->device, 1, &strucBindInfo));

      VkMemoryRequirements2KHR scratchReq;
      vCtx->vkGetAccelerationStructureScratchMemoryRequirementsNVX(vCtx->device, &memReqInfo, &scratchReq);

      VkBufferCreateInfo scratchBufInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
      scratchBufInfo.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;
      scratchBufInfo.size = scratchReq.memoryRequirements.size;
      scratchBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      CHECK_VULKAN(vkCreateBuffer(vCtx->device, &scratchBufInfo, nullptr, &md.acc.resource->scratchBuffer));

      VkMemoryAllocateInfo scratchAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
      scratchAllocInfo.memoryTypeIndex = 0;
      scratchAllocInfo.allocationSize = scratchReq.memoryRequirements.size;
      CHECK_BOOL(vCtx->resources->getMemoryTypeIndex(scratchAllocInfo.memoryTypeIndex, scratchReq.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
      CHECK_VULKAN(vkAllocateMemory(vCtx->device, &scratchAllocInfo, NULL, &md.acc.resource->scratchMem));


      CHECK_VULKAN(vkBindBufferMemory(vCtx->device, md.acc.resource->scratchBuffer, md.acc.resource->scratchMem, 0));

      auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(frame.commandPool).resource->cmdBuf;

      VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(cmdBuf, &beginInfo);

      vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                               0, VK_NULL_HANDLE, 0,
                                               1, &geometry, 0, //VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX,
                                               VK_FALSE, md.acc.resource->acc, VK_NULL_HANDLE, md.acc.resource->scratchBuffer, 0);

      vkEndCommandBuffer(cmdBuf);

      VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &cmdBuf;
      vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
      vkQueueWaitIdle(vCtx->queue);
      vkDeviceWaitIdle(vCtx->device);

      //VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
      //memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
      //memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
      //vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    }
  }
  meshData.swap(newMeshData);

  if (change && meshData.any()) {
    auto instances = meshData.size32();

    topLevel = vCtx->resources->createAccelerationStructure();

    VkAccelerationStructureCreateInfoNVX accStruInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX };
    accStruInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX;
    accStruInfo.instanceCount = instances;
    accStruInfo.geometryCount = 0;
    CHECK_VULKAN(vCtx->vkCreateAccelerationStructureNVX(vCtx->device, &accStruInfo, nullptr, &topLevel.resource->acc));

    VkMemoryRequirements2KHR strucMemReq;
    VkAccelerationStructureMemoryRequirementsInfoNVX memReqInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX };
    memReqInfo.accelerationStructure = topLevel.resource->acc;
    vCtx->vkGetAccelerationStructureMemoryRequirementsNVX(vCtx->device, &memReqInfo, &strucMemReq);

    VkMemoryAllocateInfo strucAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    strucAllocInfo.memoryTypeIndex = 0;
    strucAllocInfo.allocationSize = strucMemReq.memoryRequirements.size;
    CHECK_BOOL(vCtx->resources->getMemoryTypeIndex(strucAllocInfo.memoryTypeIndex, strucMemReq.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VULKAN(vkAllocateMemory(vCtx->device, &strucAllocInfo, NULL, &topLevel.resource->structureMem));

    VkMemoryRequirements2KHR scratchReq;
    vCtx->vkGetAccelerationStructureScratchMemoryRequirementsNVX(vCtx->device, &memReqInfo, &scratchReq);

    VkBufferCreateInfo scratchBufInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    scratchBufInfo.usage = VK_BUFFER_USAGE_RAYTRACING_BIT_NVX;
    scratchBufInfo.size = scratchReq.memoryRequirements.size;
    scratchBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CHECK_VULKAN(vkCreateBuffer(vCtx->device, &scratchBufInfo, nullptr, &topLevel.resource->scratchBuffer));

    VkMemoryAllocateInfo scratchAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    scratchAllocInfo.memoryTypeIndex = 0;
    scratchAllocInfo.allocationSize = scratchReq.memoryRequirements.size;
    CHECK_BOOL(vCtx->resources->getMemoryTypeIndex(scratchAllocInfo.memoryTypeIndex, scratchReq.memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    CHECK_VULKAN(vkAllocateMemory(vCtx->device, &scratchAllocInfo, NULL, &topLevel.resource->scratchMem));
    CHECK_VULKAN(vkBindBufferMemory(vCtx->device, topLevel.resource->scratchBuffer, topLevel.resource->scratchMem, 0));

    topLevelInstances = vCtx->resources->createBuffer(instances * sizeof(Instance), VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    {
      MappedBuffer<Instance> map(vCtx, topLevelInstances);
      //for (unsigned k = 0; k < instances; k++) {
      auto & instance = *map.mem;// (*map.mem)[k];
      for (unsigned j = 0; j < 3; j++) {
        for (unsigned i = 0; i < 4; i++) {
          instance.transform[4 * j + i] = i == j ? 1.f : 0.f;
        }
        instance.instanceID = 0;
        instance.instanceMask = 0xff;
        instance.instanceContributionToHitGroupIndex = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
        CHECK_VULKAN(vCtx->vkGetAccelerationStructureHandleNVX(vCtx->device, meshData[0].acc.resource->acc, sizeof(uint64_t), &instance.accelerationStructureHandle));
      }
      //}
    }
    assert(instances == 1);
    //VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    //memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //memoryBarrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

#if 1
    auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(frame.commandPool).resource->cmdBuf;

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                             instances, topLevelInstances.resource->buffer, 0,
                                             0, nullptr, 0, //VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX,
                                             VK_FALSE, topLevel.resource->acc, VK_NULL_HANDLE, topLevel.resource->scratchBuffer, 0);

    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    vkQueueSubmit(vCtx->queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vCtx->queue);

    vkDeviceWaitIdle(vCtx->device);
#endif
    //VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    //memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
  }
#endif
}


void Raycaster::resize(const Vec4f& viewport)
{
  auto newW = uint32_t(viewport.z < 1 ? 1.f : viewport.z);
  auto newH = uint32_t(viewport.w < 1 ? 1.f : viewport.w);
  if (newW == w && newH == h) return;
  w = newW;
  h = newH;
  logger(0, "Resized to %dx%d", w, h);

  auto * vCtx = vulkanManager->vCtx;
  auto * resources = vCtx->resources;
  for (auto & rename : renames) {

    VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    imageInfo.extent = { w, h, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rename.offscreenImage = resources->createImage(imageInfo);

    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    rename.offscreenView = resources->createImageView(rename.offscreenImage, viewInfo);
  }

}


void Raycaster::draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat4f& Pinv)
{
  resize(viewport);

  renameIndex = renameIndex + 1;
  if (renames.size32() <= renameIndex) renameIndex = 0;
  auto & rename = renames[renameIndex];

  auto * vCtx = vulkanManager->vCtx;

  auto offscreenImage = rename.offscreenImage.resource->image;
  auto onscreenImage = vCtx->frameManager->backBufferImages[vCtx->frameManager->swapChainIndex];

  { // Prep for shader storage
    VkImageMemoryBarrier imageMemoryBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = offscreenImage;
    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
  if (false) {
    vCtx->vkCmdTraceRaysNVX(cmdBuf,
                            bindingTable.resource->buffer, 0 * rtProps.shaderHeaderSize,      // raygen
                            bindingTable.resource->buffer, 1 * rtProps.shaderHeaderSize, 0,   // miss
                            bindingTable.resource->buffer, 2 * rtProps.shaderHeaderSize, 0,   // hit
                            uint32_t(viewport.z), uint32_t(viewport.w));

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
    imageMemoryBarriers[0].image = offscreenImage;
    imageMemoryBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    imageMemoryBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarriers[1].srcAccessMask = 0;// srcAccessMask;
    imageMemoryBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarriers[1].image = onscreenImage;
    imageMemoryBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 2, imageMemoryBarriers);
  }
  { // blit
    VkImageCopy imageCopy{};
    imageCopy.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    imageCopy.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    imageCopy.dstOffset = { 0, 0, 0 };
    imageCopy.extent = { w, h, 1 };
    vkCmdCopyImage(cmdBuf,
                   offscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   onscreenImage, VK_IMAGE_LAYOUT_GENERAL /*VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL*/, 1, &imageCopy);
  }
  { // Prop for presentiation
    VkImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = 0;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;// VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.image = onscreenImage;
    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }
}
