#pragma once
#include <mutex>
#include <list>
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

  bool updateColor = true;
  bool selectAll = false;
  bool selectNone = false;
  bool viewAll = false;
  bool moveToSelection = false;
  bool picking = false;

  bool colorFromSmoothingGroup = true;
  bool colorFromObjectId = false;

  VulkanManager* vulkanManager = nullptr;


  std::mutex incomingMeshLock;
  std::list<Mesh*> incomingMeshes;

  Vector<RenderMeshHandle> renderMeshes;

  std::list<MeshItem> meshItems;

};