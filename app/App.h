#pragma once
#include <mutex>
#include "Common.h"
#include "RenderSolid.h"
#include "Tasks.h"

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
class RenderOutlines;
class RenderTangents;
class RenderNormals;
class RenderSolid;
class Raycaster;
class ImGuiRenderer;
struct GLFWwindow;

enum TriangleColor
{
  Single,
  ModelColor,
  ObjectId,
  SmoothingGroup,
  TriangleOrder,
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

  TriangleColor triangleColor = TriangleColor::TriangleOrder;
  bool raytrace = false;

  bool viewSolid = true;
  bool viewLines = true;
  bool viewOutlines = false;
  bool viewNormals = false;
  bool viewTangents = false;

  bool updateColor = true;
  bool selectAll = false;
  bool selectNone = false;
  bool viewAll = false;
  bool moveToSelection = false;
  bool picking = false;
  unsigned scrollToItem = ~0u;

  char fpsString[64] = { '\0' };

  std::mutex incomingMeshLock;
  Vector<Mesh*> incomingMeshes;

  struct {
    Vector<Mesh*> meshes;
  } items;

  Logger logger;
  GLFWwindow* window = nullptr;
  Raycaster* raycaster = nullptr;
  RenderSolid* renderSolid = nullptr;
  RenderOutlines* renderOutlines = nullptr;
  RenderTangents* renderTangents = nullptr;
  RenderNormals* renderNormals = nullptr;
  ImGuiRenderer* imGuiRenderer = nullptr;
  VulkanContext* vCtx = nullptr;

  RenderPassHandle rendererPass;
  RenderPassHandle imguiRenderPass;

  //Vector<ImageViewHandle> backBufferViews;

  Vector<FrameBufferHandle> rendererFrameBuffers;
  Vector<FrameBufferHandle> imguiFrameBuffers;


  double fpsStart = 0.0;
  double cpuTimeAcc = 0.0;
  double gpuTimeAcc = 0.0;
  unsigned cpuTimeAccN = 0;
  unsigned gpuTimeAccN = 0;


};