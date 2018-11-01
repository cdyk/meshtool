#include "Raycaster.h"
#include "VulkanManager.h"
#include "VulkanContext.h"
#include "LinAlgOps.h"
#include "Mesh.h"

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
  };

  struct TriangleData
  {
    float n0x, n0y, n0z;
    float n1x, n1y, n1z;
    float n2x, n2y, n2z;
    float r, g, b;
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
  renames.resize(5);

  auto * vCtx = vulkanManager->vCtx;
  VkDescriptorPoolSize poolSizes[] =
  {
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, renames.size32()},
    { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, renames.size32() },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  10 * renames.size32() },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 * renames.size32()}
  };

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.pNext = nullptr;
  descriptorPoolCreateInfo.flags = 0;
  descriptorPoolCreateInfo.maxSets = renames.size32();
  descriptorPoolCreateInfo.poolSizeCount = ARRAYSIZE(poolSizes);
  descriptorPoolCreateInfo.pPoolSizes = poolSizes;
  CHECK_VULKAN(vkCreateDescriptorPool(vCtx->device, &descriptorPoolCreateInfo, nullptr, &descPool));
}


VkDescriptorSetLayout Raycaster::buildDescriptorSetLayout()
{
  auto * vCtx = vulkanManager->vCtx;
  VkDescriptorSetLayoutBinding descSetLayoutBinding[4];
  descSetLayoutBinding[0] = {};
  descSetLayoutBinding[0].binding = 0;
  descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
  descSetLayoutBinding[0].descriptorCount = 1;
  descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;

  descSetLayoutBinding[1] = {};
  descSetLayoutBinding[1].binding = 1;
  descSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descSetLayoutBinding[1].descriptorCount = 1;
  descSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;

  descSetLayoutBinding[2] = {};
  descSetLayoutBinding[2].binding = 2;
  descSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descSetLayoutBinding[2].descriptorCount = 1;
  descSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX | VK_SHADER_STAGE_MISS_BIT_NVX;

  descSetLayoutBinding[3] = {};
  descSetLayoutBinding[3].binding = 3;
  descSetLayoutBinding[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descSetLayoutBinding[3].descriptorCount = meshData.size32();
  descSetLayoutBinding[3].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;

  VkDescriptorBindingFlagsEXT flags[4] = { 0, 0, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT };

  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT };
  bindingFlags.pBindingFlags = flags;
  bindingFlags.bindingCount = ARRAYSIZE(flags);

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
  layoutCreateInfo.pNext = &bindingFlags;
  layoutCreateInfo.bindingCount = sizeof(descSetLayoutBinding) / sizeof(descSetLayoutBinding[0]);
  layoutCreateInfo.pBindings = descSetLayoutBinding;
  CHECK_VULKAN(vkCreateDescriptorSetLayout(vCtx->device, &layoutCreateInfo, nullptr, &layout));
  CHECK_VULKAN(vkResetDescriptorPool(vCtx->device, descPool, 0));
  for (auto & rename : renames) {
    rename.descSet = VK_NULL_HANDLE;
  }
  return layout;
}


void Raycaster::buildPipeline()
{
  auto * vCtx = vulkanManager->vCtx;

  rtProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAYTRACING_PROPERTIES_NVX };
  VkPhysicalDeviceProperties2 props2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
  props2.pNext = &rtProps;
  vkGetPhysicalDeviceProperties2(vCtx->physicalDevice, &props2);

  logger(0, "rtprops.shaderHeaderSize=%d", rtProps.shaderHeaderSize);
  logger(0, "rtprops.maxRecursionDepth=%d", rtProps.maxRecursionDepth);
  logger(0, "rtprops.maxGeometryCount=%d", rtProps.maxGeometryCount);


  Vector<ShaderInputSpec> stages(3);
  stages[0] = { raytrace_rgen, sizeof(raytrace_rgen), VK_SHADER_STAGE_RAYGEN_BIT_NVX };
  stages[1] = { raytrace_rchit, sizeof(raytrace_rchit), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX };
  stages[2] = { raytrace_rmiss, sizeof(raytrace_rmiss), VK_SHADER_STAGE_MISS_BIT_NVX };

  uint32_t groupNumbers[4] = { 0, 1, 2 };

  shader = vCtx->resources->createShader(stages);
  assert(shader.resource->stageCreateInfo.size() == stages.size());

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

    VkRaytracingPipelineCreateInfoNVX info = { VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX };
    info.pStages = shader.resource->stageCreateInfo.data();
    info.stageCount = shader.resource->stageCreateInfo.size32();
    info.pGroupNumbers = groupNumbers;
    info.maxRecursionDepth = 4;
    info.layout = pipe->pipeLayout;
    CHECK_VULKAN(vCtx->vkCreateRaytracingPipelinesNVX(vCtx->device, nullptr /*vCtx->pipelineCache*/, 1, &info, nullptr, &pipe->pipe));
  }

  //CHECK_VULKAN(vCtx->vkCompileDeferredNVX(vCtx->device, pipe->pipe, 0));
  { // shader binding table

    uint32_t groupCount = 3;
    uint32_t tableSize = groupCount * rtProps.shaderHeaderSize;
    
    bindingTable = vCtx->resources->createBuffer(tableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    {
      char* ptr = nullptr;
      MappedBufferBase map((void**)&ptr, vCtx, bindingTable);

      CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 0, groupCount, tableSize, ptr));
      //CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 1, 1, bindingTableSize, ptr + 1 * rtProps.shaderHeaderSize));
      //CHECK_VULKAN(vCtx->vkGetRaytracingShaderHandlesNVX(vCtx->device, pipe->pipe, 2, 1, bindingTableSize, ptr + 2 * rtProps.shaderHeaderSize));
      // instanceShaderBindingTableRecordOffset stored in each instance of top-level acc struc
      // geometry index is the geometry within the instance.
      // instanceShaderBindingTableRecordOffset + hitProgramShaderBindingTableBaseIndex + geometryIndex × sbtRecordStride + sbtRecordOffset
    }
  }

  
}


void Raycaster::buildDescriptorSets()
{
  auto * vCtx = vulkanManager->vCtx;
  auto * pipe = pipeline.resource;

  

  for (auto & rename : renames) {
    rename.sceneBuffer = vCtx->resources->createUniformBuffer(sizeof(SceneBuffer));

    if (!rename.descSet) {

      // an array of descriptor counts, with each member specifying the number of descriptors in a
      // variable descriptor count binding in the corresponding descriptor set being allocated.
      uint32_t varDescCount = meshData.size32();

      VkDescriptorSetVariableDescriptorCountAllocateInfoEXT varDescInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT };
      varDescInfo.descriptorSetCount = 1;
      varDescInfo.pDescriptorCounts = &varDescCount;

      VkDescriptorSetAllocateInfo descAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
      descAllocInfo.pNext = &varDescInfo;
      descAllocInfo.descriptorPool = descPool;
      descAllocInfo.descriptorSetCount = 1;
      descAllocInfo.pSetLayouts = &pipe->descLayout;
      CHECK_VULKAN(vkAllocateDescriptorSets(vCtx->device, &descAllocInfo, &rename.descSet));
    }

    VkDescriptorAccelerationStructureInfoNVX descriptorAccelerationStructureInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX };
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topAcc.resource->acc;

    Vector<VkWriteDescriptorSet> writes(3);
    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[0].pNext = &descriptorAccelerationStructureInfo;
    writes[0].dstSet = rename.descSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    writes[0].pImageInfo = nullptr;
    writes[0].pBufferInfo = nullptr;
    writes[0].pTexelBufferView = nullptr;

    writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[1].dstSet = rename.descSet;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].pBufferInfo = &rename.sceneBuffer.resource->descInfo;
    writes[1].dstArrayElement = 0;
    writes[1].dstBinding = 2;

    Vector<VkDescriptorBufferInfo> bufInfo(meshData.size32());
    for (uint32_t g = 0; g < meshData.size32(); g++) {
      bufInfo[g].buffer = meshData[g].triangleData.resource->buffer;
      bufInfo[g].offset = 0;
      bufInfo[g].range = VK_WHOLE_SIZE;
    }
    writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    writes[2].dstSet = rename.descSet;
    writes[2].descriptorCount = bufInfo.size32();
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = bufInfo.data();
    writes[2].dstArrayElement = 0;
    writes[2].dstBinding = 3;

    vkUpdateDescriptorSets(vCtx->device, writes.size32(), writes.data(), 0, nullptr);
  }

}


bool Raycaster::updateMeshData(VkDeviceSize& scratchSize, MeshData& meshData, const RenderMeshHandle& renderMesh)
{
  auto * vCtx = vulkanManager->vCtx;
  auto * res = vCtx->resources;

  auto * rm = renderMesh.resource;
  if (meshData.meshGen == rm->generation) return false;
  meshData.meshGen = rm->generation;

  logger(0, "Updating MeshData item.");
  auto * mesh = rm->mesh;

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
    MappedBuffer<TriangleData> map(vCtx, meshData.triangleData);
    for (uint32_t t = 0; t < meshData.triangleCount; t++) {
      auto & data = map.mem[t];
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

  auto & geometry = meshData.geometry;
  geometry = { VK_STRUCTURE_TYPE_GEOMETRY_NVX };

  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
  geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
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
  geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX;
  geometry.flags = 0;

  meshData.acc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX, 1, &geometry, 0);
  if (scratchSize < meshData.acc.resource->scratchReqs.size) scratchSize = meshData.acc.resource->scratchReqs.size;

  meshData.rebuild = true;
  return true;
}


void Raycaster::update(Vector<RenderMeshHandle>& meshes)
{
  auto * vCtx = vulkanManager->vCtx;
  auto & frame = vCtx->frameManager->frame();

  VkDeviceSize scratchSize = 0;


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
    if (updateMeshData(scratchSize, newMeshData.back(), renderMesh)) {
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

    Vector<VkGeometryNVX> geometries(geometryCount);
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
      CHECK_VULKAN(vCtx->vkGetAccelerationStructureHandleNVX(vCtx->device, meshData[g].acc.resource->acc, sizeof(uint64_t), &instance.accelerationStructureHandle));
    }
    auto instanceBuffer = vCtx->resources->createBuffer(sizeof(Instance)*geometryCount, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vCtx->resources->copyHostMemToBuffer(instanceBuffer, instances.data(), sizeof(Instance)*geometryCount);

    topAcc = vCtx->resources->createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX, 0, nullptr, uint32_t(instances.size()));
    if (scratchSize < topAcc.resource->scratchReqs.size) scratchSize = topAcc.resource->scratchReqs.size;

    auto scratchBuffer = vCtx->resources->createBuffer(scratchSize, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;

    auto cmdBuf = vCtx->resources->createPrimaryCommandBuffer(frame.commandPool).resource->cmdBuf;
    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    for (unsigned g = 0; g < geometryCount; g++) {
      if (meshData[g].rebuild) {
        meshData[g].rebuild = false;
        vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                                 0, VK_NULL_HANDLE, 0,
                                                 (uint32_t)geometries.size(), &geometries[0],
                                                 0, VK_FALSE, meshData[g].acc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);
        vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
      }
    }
    vCtx->vkCmdBuildAccelerationStructureNVX(cmdBuf, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                             1, instanceBuffer.resource->buffer, 0,
                                             0, nullptr,
                                             0, VK_FALSE, topAcc.resource->acc, VK_NULL_HANDLE, scratchBuffer.resource->buffer, 0);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
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
    vkDeviceWaitIdle(vCtx->device);

    buildPipeline();
    buildDescriptorSets();
  }

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


    VkDescriptorImageInfo descImageInfo;
    descImageInfo.sampler = nullptr;
    descImageInfo.imageView = rename.offscreenView.resource->view;
    descImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet descImageWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descImageWrite.dstSet = rename.descSet;
    descImageWrite.dstBinding = 1;
    descImageWrite.dstArrayElement = 0;
    descImageWrite.descriptorCount = 1;
    descImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descImageWrite.pImageInfo = &descImageInfo;
    vkUpdateDescriptorSets(vCtx->device, (uint32_t)1, &descImageWrite, 0, nullptr);
  }
}


void Raycaster::draw(VkCommandBuffer cmdBuf, const Vec4f& viewport, const Mat3f& Ninv, const Mat4f& Pinv)
{
  if (!topAcc) return;

  resize(viewport);

  renameIndex = renameIndex + 1;
  if (renames.size32() <= renameIndex) renameIndex = 0;
  auto & rename = renames[renameIndex];
  auto * vCtx = vulkanManager->vCtx;
  {
    auto l = normalize(mul(Ninv, Vec3f(1, 1, 1)));
    auto u = normalize(mul(Ninv, Vec3f(0, 1, 0)));

    MappedBuffer<SceneBuffer> map(vCtx, rename.sceneBuffer);
    map.mem->Pinv = Pinv;
    map.mem->lx = l.x;
    map.mem->ly = l.y;
    map.mem->lz = l.z;
    map.mem->ux = u.x;
    map.mem->uy = u.y;
    map.mem->uz = u.z;
  }

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
  {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, pipeline.resource->pipe);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAYTRACING_NVX, pipeline.resource->pipeLayout, 0, 1, &rename.descSet, 0, 0);
    vCtx->vkCmdTraceRaysNVX(cmdBuf,
                            bindingTable.resource->buffer, 0,      // raygen
                            bindingTable.resource->buffer, 2 * rtProps.shaderHeaderSize, rtProps.shaderHeaderSize,   // miss
                            bindingTable.resource->buffer, 1 * rtProps.shaderHeaderSize, rtProps.shaderHeaderSize,   // hit
                            w, h);

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
    imageMemoryBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
    imageCopy.dstOffset = { int32_t(viewport.x), int32_t(viewport.y), 0 };
    imageCopy.extent = { w, h, 1 };
    vkCmdCopyImage(cmdBuf,
                   offscreenImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   onscreenImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);
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
}
