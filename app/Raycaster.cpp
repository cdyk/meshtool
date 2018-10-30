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


  Vector<ShaderInputSpec> stages(4);
  stages[0] = { raytrace_rgen, sizeof(raytrace_rgen), VK_SHADER_STAGE_RAYGEN_BIT_NVX };
  stages[1] = { raytrace_rmiss, sizeof(raytrace_rmiss), VK_SHADER_STAGE_MISS_BIT_NVX };
  stages[2] = { raytrace_rchit, sizeof(raytrace_rchit), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX };
  stages[3] = { raytrace_rahit, sizeof(raytrace_rahit), VK_SHADER_STAGE_ANY_HIT_BIT_NVX };

  Vector<uint32_t> groupNumbers(stages.size());
  groupNumbers[0] = 0;
  groupNumbers[1] = 1;
  groupNumbers[2] = 2;
  groupNumbers[3] = 2;

  shader = vCtx->resources->createShader(stages);
  assert(shader.resource->stageCreateInfo.size() == stages.size());

  pipeline = vCtx->resources->createPipeline();
  auto * pipe = pipeline.resource;


  {
    VkDescriptorSetLayoutBinding descSetLayoutBinding[2];
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
    // instanceShaderBindingTableRecordOffset + hitProgramShaderBindingTableBaseIndex + geometryIndex × sbtRecordStride + sbtRecordOffset
  }

  bindingTable = vCtx->resources->createBuffer(bindingTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  vCtx->frameManager->copyBuffer(bindingTable, bindingTableStage, bindingTableSize);

}

void Raycaster::update(VkCommandBuffer cmdBuf, Vector<RenderMeshHandle>& meshes)
{
  auto * vCtx = vulkanManager->vCtx;

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

      vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                               0, VK_NULL_HANDLE, 0,
                                               1, &geometry, 0, //VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX,
                                               VK_FALSE, md.acc.resource->acc, VK_NULL_HANDLE, md.acc.resource->scratchBuffer, 0);

      VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
      memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
      memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;

      vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
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
    vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                             instances, topLevelInstances.resource->buffer, 0,
                                             0, nullptr, 0, //VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NVX,
                                             VK_FALSE, topLevel.resource->acc, VK_NULL_HANDLE, topLevel.resource->scratchBuffer, 0);
#endif
    //VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    //memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    //vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
  }
}


void Raycaster::draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat4f& Pinv)
{
  auto * vCtx = vulkanManager->vCtx;

  return;
  vCtx->vkCmdTraceRaysNVX(cmdBuf,
                          bindingTable.resource->buffer, 0 * rtProps.shaderHeaderSize,      // raygen
                          bindingTable.resource->buffer, 1 * rtProps.shaderHeaderSize, 0,   // miss
                          bindingTable.resource->buffer, 2 * rtProps.shaderHeaderSize, 0,   // hit
                          uint32_t(viewport.z), uint32_t(viewport.w));
}
