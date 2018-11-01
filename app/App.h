#pragma once
#include <mutex>
#include "Common.h"
#include "Renderer.h"


struct MeshItem
{
  Mesh* mesh;
  RenderMeshHandle renderMesh;
};

struct Mesh;
class VulkanManager;
class Viewer;

enum TriangleColor
{
  Single,
  ModelColor,
  ObjectId,
  SmoothingGroup
};

class App
{
public:
  App();
  ~App();

  Viewer* viewer;
  Tasks tasks;
  bool wasResized = false;
  int width, height;
  float leftSplit = 100;
  float thickness = 8;
  float menuHeight = 0.f;

  TriangleColor triangleColor = TriangleColor::ModelColor;
  bool raytrace = false;
  bool updateColor = true;
  bool selectAll = false;
  bool selectNone = false;
  bool viewAll = false;
  bool moveToSelection = false;
  bool picking = false;
  unsigned scrollToItem = ~0u;

  VulkanManager* vulkanManager = nullptr;


  std::mutex incomingMeshLock;
  Vector<Mesh*> incomingMeshes;

  struct {
    Vector<Mesh*> meshes;
    Vector<RenderMeshHandle> renderMeshes;
  } items;

};