#pragma once
#include <mutex>
#include "Common.h"
#include "Renderer.h"

#if 0

#include <list>
#include "Common.h"
#include "VulkanResources.h"
#include "RenderMeshManager.h"

struct RenderMesh;
struct Vec4f;
struct Mat4f;
struct Mat3f;

class Renderer;
class Raycaster;
class ImGuiRenderer;
struct GLFWwindow;

#endif


struct Mesh;
class Viewer;
class Renderer;
class Raycaster;
class ImGuiRenderer;
struct GLFWwindow;

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
  App(Logger l, GLFWwindow* window, uint32_t w, uint32_t h);
  ~App();

  void resize(uint32_t w, uint32_t h);

  void startFrame();

  void render(const Vec4f& viewport);

  void present();

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


  std::mutex incomingMeshLock;
  Vector<Mesh*> incomingMeshes;

  struct {
    Vector<Mesh*> meshes;
  } items;

  Logger logger;
  GLFWwindow* window = nullptr;
  Renderer* renderer = nullptr;
  Raycaster* raycaster = nullptr;
  ImGuiRenderer* imGuiRenderer = nullptr;
  VulkanContext* vCtx = nullptr;

  RenderPassHandle rendererPass;
  RenderPassHandle imguiRenderPass;

  //Vector<ImageViewHandle> backBufferViews;

  Vector<FrameBufferHandle> rendererFrameBuffers;
  Vector<FrameBufferHandle> imguiFrameBuffers;


};