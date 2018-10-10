#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
//#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <cctype>
#include <chrono>
#include <examples/imgui_impl_glfw.h>

#include <cstdio>
#include "Viewer.h"
#include "Common.h"
#include "Mesh.h"
#include "LinAlgOps.h"
#include "Renderer.h"
#include "VulkanManager.h"

#include "App.h"


namespace {

  
  App* app = new App();
  

  

  void logger(unsigned level, const char* msg, ...)
  {
    switch (level) {
    case 0: fprintf(stderr, "[I] "); break;
    case 1: fprintf(stderr, "[W] "); break;
    case 2: fprintf(stderr, "[E] "); break;
    }

    va_list argptr;
    va_start(argptr, msg);
    vfprintf(stderr, msg, argptr);
    va_end(argptr);
    fprintf(stderr, "\n");
  }

  void glfw_error_callback(int error, const char* description)
  {
    logger(2, "GLFV error %d: %s", error, description);
  }

  void resizeFunc(GLFWwindow* window, int w, int h)
  {
    app->wasResized = true;
    app->width = w;
    app->height = h;
  }

  void moveFunc(GLFWwindow* window, double x, double y)
  {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    app->viewer->move(float(x), float(y));
  }

  void buttonFunc(GLFWwindow* window, int button, int action, int mods)
  {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    switch (action) {
    case GLFW_PRESS:
      switch (button) {
      case 0:
        if (mods == GLFW_MOD_CONTROL) {
          app->picking = true;
        }
        else {
          app->viewer->startRotation(float(x), float(y));
        }
        break;
      case 1: app->viewer->startZoom(float(x), float(y)); break;
      case 2: app->viewer->startPan(float(x), float(y)); break;
      }
      break;
    case GLFW_RELEASE:
      app->viewer->stopAction();
      break;
    }
  }

  void keyFunc(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    if (mods == GLFW_MOD_CONTROL) {
      if (key == GLFW_KEY_A && action == GLFW_PRESS) app->selectAll = true;
      if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) app->selectNone = true;
    }
    else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT)) {
      if (key == GLFW_KEY_A && action == GLFW_PRESS) app->viewAll = true;
      if (key == GLFW_KEY_S && action == GLFW_PRESS) app->moveToSelection = true;
    }
    else if (mods == 0) {
      if (key == GLFW_KEY_W && action == GLFW_PRESS)  app->vulkanManager->renderer->outlines = !app->vulkanManager->renderer->outlines;
      if (key == GLFW_KEY_S && action == GLFW_PRESS)  app->vulkanManager->renderer->solid = !app->vulkanManager->renderer->solid;
    }

  }

  void scrollFunc(GLFWwindow* window, double x, double y)
  {
    float speed = 1.f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
      speed = 0.1f;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
      speed = 0.01f;
    }

    bool distance = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;

    app->viewer->dolly(float(x), float(y), speed, distance);
  }

  void runObjReader(Logger logger, std::string path)
  {
    auto time0 = std::chrono::high_resolution_clock::now();
    Mesh* mesh = nullptr;
    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
      logger(2, "Failed to open file %s: %d", path.c_str(), GetLastError());
    }
    else {
      DWORD hiSize;
      DWORD loSize = GetFileSize(h, &hiSize);
      size_t fileSize = (size_t(hiSize) << 32u) + loSize;

      HANDLE m = CreateFileMappingA(h, 0, PAGE_READONLY, 0, 0, NULL);
      if (m == INVALID_HANDLE_VALUE) {
        logger(2, "Failed to map file %s: %d", path.c_str(), GetLastError());
      }
      else {
        const void * ptr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
        if (ptr == nullptr) {
          logger(2, "Failed to map view of file %s: %d", path.c_str(), GetLastError());
        }
        else {
          mesh = readObj(logger, ptr, fileSize);
          UnmapViewOfFile(ptr);
        }
        CloseHandle(m);
      }
      CloseHandle(h);
    }
    if (mesh) {

      auto * name = path.c_str();
      for (auto * t = name; *t != '\0'; t++) {
        if (*t == '\\' || *t == '//') name = t + 1;
      }
      mesh->name = mesh->strings.intern(name);

      auto time1 = std::chrono::high_resolution_clock::now();
      auto e = std::chrono::duration_cast<std::chrono::milliseconds>((time1 - time0)).count();
      std::lock_guard<std::mutex> guard(app->incomingMeshLock);
      app->incomingMeshes.push_back(mesh);
      logger(0, "Read %s in %lldms", path.c_str(), e);
    }
    else {
      logger(0, "Failed to read %s", path.c_str());
    }
  }

  void guiTraversal(GLFWwindow* window)
  {
    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", nullptr, nullptr)) { glfwSetWindowShouldClose(window, true); }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Select")) {
        if (ImGui::MenuItem("All", "CTRL+A" )) {
          app->selectAll = true;
        }
        if (ImGui::MenuItem("None", "CTRL+ ")) {
          app->selectNone = true;
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("View all", "CTRL+SHIFT+A", nullptr)) { app->viewAll = true; }
        if (ImGui::MenuItem("View selection", "CTRL+SHIFT+S")) { app->moveToSelection = true; }
        ImGui::Separator();
        if (ImGui::MenuItem("Solid", "S", &app->vulkanManager->renderer->solid)) {}
        if (ImGui::MenuItem("Outlines", "W", &app->vulkanManager->renderer->outlines)) {}
        ImGui::Separator();
        if (ImGui::MenuItem("Color from smoothing group", nullptr, &app->colorFromSmoothingGroup)) {
          app->colorFromObjectId = false;
          app->updateColor = true;
        }
        if (ImGui::MenuItem("Color from object id", nullptr, &app->colorFromObjectId)) {
          app->colorFromSmoothingGroup = false;
          app->updateColor = true;
        }
        ImGui::EndMenu();
      }
      app->menuHeight = ImGui::GetWindowSize().y;
      ImGui::EndMainMenuBar();
    }


    {
      ImGui::SetNextWindowPos(ImVec2(0, app->menuHeight));
      ImGui::SetNextWindowSize(ImVec2(app->leftSplit, app->height - app->menuHeight));
      ImGui::Begin("##left", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);


      //ImGuiContext& g = *GImGui;
      //ImGuiWindow* window = g.CurrentWindow;

      ImVec2 backup_pos = ImGui::GetCursorPos();
      ImGui::SetCursorPosX(app->leftSplit - app->thickness);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.10f));
      ImGui::Button("##Splitter", ImVec2(app->thickness, -1));

      ImGui::PopStyleColor(3);
      if (ImGui::IsItemActive()) {
        app->leftSplit += ImGui::GetIO().MouseDelta.x;
        if (app->leftSplit < 50) app->leftSplit = 50;
        if (app->width - app->thickness < app->leftSplit) app->leftSplit = app->width - app->thickness;
      }

      ImGui::SetCursorPos(backup_pos);
      ImGui::BeginChild("Meshes", ImVec2(app->leftSplit - app->thickness - backup_pos.x, -1), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
      for(auto & item : app->meshItems) {
        auto * m = item.mesh;
        if (ImGui::TreeNodeEx(m, ImGuiTreeNodeFlags_DefaultOpen, "%s Vn=%d Tn=%d", m->name ? m->name : "unnamed", m->vtxCount, m->triCount)) {
          for (unsigned o = 0; o < m->obj_n; o++) {
            if (ImGui::Selectable(m->obj[o], &m->selected[o], ImGuiSelectableFlags_PressedOnClick)) {
              app->updateColor = true;
              //moveToSelection = true;
            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::EndChild();
      ImGui::End();
    }
    //ImGui::ShowDemoWindow();
  }

  void checkQueues()
  {
    std::list<Mesh*> newMeshes;
    {
      std::lock_guard<std::mutex> guard(app->incomingMeshLock);
      if (!app->incomingMeshes.empty()) {
        newMeshes = std::move(app->incomingMeshes);
        app->incomingMeshes.clear();
      }
    }
    if (!newMeshes.empty()) {
      for (auto & m : newMeshes) {

        auto obj_n = m->obj_n;
        if (obj_n == 0) obj_n = 1;

        m->currentColor = (uint32_t*)m->arena.alloc(sizeof(uint32_t) * m->triCount);
        m->selected = (bool*)m->arena.alloc(sizeof(bool)*(obj_n));
        std::memset(m->selected, 0, sizeof(bool)*obj_n);
        app->updateColor = true;

        app->meshItems.push_back({ m, app->vulkanManager->renderer->createRenderMesh(m) });
      }
      auto bbox = createEmptyBBox3f();
      for (auto & item : app->meshItems) {
        auto * m = item.mesh;
        if (isEmpty(m->bbox)) continue;
        engulf(bbox, m->bbox);
      }
      if (isEmpty(bbox)) {
        bbox = BBox3f(Vec3f(-1.f), Vec3f(1.f));
      }
      app->viewer->setViewVolume(bbox);
      app->viewer->viewAll();
    }
  }

}

namespace {

  uint32_t colors[] = {
    0xffcc0000,0xff9e91cc,0xff5ea5f4,0xff8c8c51,
    0xffff7547,0xff2b2bcc,0xffe0eded,0xff2d4f2d,
    0xff707ff9,0xff00a5ff,0xff2175ed,0xff0000ff,
    0xff8c668c,0xff238e23,0xffaae8ed,0xff445bcc,
    0xff660033,0xff937c68,0xff33c9ed,0xffd1eded,
    0xffe5e0af,0xffbfbfbf,0xff00cccc,0xff14448c,
    0xff8911ed,0xff990066,0xffdd00dd,0xff4763ff,
    0xff4f2d2d,0xff0099ed,0xff33cc99,0xffc6ed75,
    0xff4f4f2d,0xff0000cc,0xff5e9e9e,0xffb2ddf4,
    0xffeded00,0xffccbf00,0xff7fff00,0xffa8a8a8,
    0xff00cc00,0xffdbf4f4,0xff007fff,0xff7093db,
    0xfff4f4f4,0xff6b238e,0xff7f0000,0xffed82ed
  };
}

int main(int argc, char** argv)
{
  GLFWwindow* window;
  app->tasks.init(logger);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return -1;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  if (!glfwVulkanSupported()) {
    logger(2, "GLFW: Vulkan not supported");
    return -1;
  }
  glfwGetFramebufferSize(window, &app->width, &app->height);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();


  app->vulkanManager = new VulkanManager(logger, window, app->width, app->height);

  glfwSetWindowSizeCallback(window, resizeFunc);
  glfwSetCursorPosCallback(window, moveFunc);
  glfwSetMouseButtonCallback(window, buttonFunc);
  glfwSetScrollCallback(window, scrollFunc);
  glfwSetKeyCallback(window, keyFunc);


  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  ImGui_ImplGlfw_InitForOpenGL(window, false);
  //ImGui_ImplOpenGL2_Init();

  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  BBox3f viewVolume(Vec3f(-2.f), Vec3f(2.f));
  app->viewer->setViewVolume(viewVolume);
  app->viewer->resize(1280, 720);



  for (int i = 1; i < argc; i++) {
    auto arg = std::string(argv[i]);
    if (arg.substr(0, 2) == "--") {
    }
    else {
      auto argLower = arg;
      for(auto & c :argLower) c = std::tolower(c);
      auto l = argLower.rfind(".obj");
      if (l != std::string::npos) {
        TaskFunc taskFunc = [arg]() {runObjReader(logger, arg); };
        app->tasks.enqueue(taskFunc);
      }
    }
  }

  app->leftSplit = 0.25f*app->width;
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if (app->wasResized) {
      app->wasResized = false;
      app->vulkanManager->resize(app->width, app->height);
    }
    {
      Vec2f viewerPos(app->leftSplit, app->menuHeight);
      Vec2f viewerSize(app->width - viewerPos.x, app->height - viewerPos.y);
      app->viewer->resize(viewerPos, viewerSize);
      app->viewer->update();
    }

    app->tasks.update();
    checkQueues();
    app->vulkanManager->renderer->houseKeep();

    app->vulkanManager->startFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    guiTraversal(window);
    ImGui::Render();

    Vec4f viewport(app->leftSplit, app->menuHeight, app->width - app->leftSplit, app->height - app->menuHeight);

    if (app->picking) {
      app->picking = false;
      double x, y;
      glfwGetCursorPos(window, &x, &y);

      logger(0, "picking at %f %f", x, y);
    }

    if (app->selectAll | app->selectNone) {
      app->selectAll = app->selectNone = false;
      for (auto & m : app->meshItems) {
        for (uint32_t i = 0; i < m.mesh->obj_n; i++) {
          m.mesh->selected[i] = app->selectAll;
          app->updateColor = true;
        }
      }
    }

    if (app->viewAll) {
      app->viewAll = false;
      app->viewer->viewAll();
    }

    if (app->moveToSelection) {
      app->moveToSelection = false;
      BBox3f bbox = createEmptyBBox3f();
      for (auto & it : app->meshItems) {
        auto * m = it.mesh;
        for (uint32_t i = 0; i < m->triCount; i++) {
          if (m->selected[m->TriObjIx[i]]) {
            for (unsigned k = 0; k < 3; k++) {
              engulf(bbox, m->vtx[m->triVtxIx[3 * i + k]]);
            }
          }
        }
      }
      if (isNotEmpty(bbox)) {
        app->viewer->view(bbox);
      }
    }

    if (app->updateColor) {
      for (auto & it : app->meshItems) {
        auto * m = it.mesh;

        if (app->colorFromSmoothingGroup && m->triSmoothGroupIx) {
          for (uint32_t i = 0; i < m->triCount; i++) {
            if (m->selected[m->TriObjIx[i]]) {
              m->currentColor[i] = 0xffddffff;
            }
            else {
              m->currentColor[i] = colors[m->triSmoothGroupIx[i] % (sizeof(colors) / sizeof(uint32_t))];
            }
          }
        }
        else if (app->colorFromObjectId && m->TriObjIx) {
          for (uint32_t i = 0; i < m->triCount; i++) {
            if (m->selected[m->TriObjIx[i]]) {
              m->currentColor[i] = 0xffddffff;
            }
            else {
              m->currentColor[i] = colors[m->TriObjIx[i] % (sizeof(colors) / sizeof(uint32_t))];
            }
          }
        }
        else {
          for (uint32_t i = 0; i < m->triCount; i++) {
            if (m->selected[m->TriObjIx[i]]) {
              m->currentColor[i] = 0xffddffff;
            }
            else {
              m->currentColor[i] = 0xffff8888;
            }
          }
        }
      }
    }

    app->renderMeshes.resize(app->meshItems.size());
    size_t i = 0; 
    for (auto & it : app->meshItems) {
      app->renderMeshes[i++] = it.renderMesh;
      if (app->updateColor) {
        app->vulkanManager->renderer->updateRenderMeshColor(it.renderMesh);
      }
    }
    app->updateColor = false;

    app->vulkanManager->render(app->width, app->height, app->renderMeshes, viewport, app->viewer->getProjectionMatrix(), app->viewer->getViewMatrix());
    app->vulkanManager->present();
  }
  app->meshItems.clear();
  app->vulkanManager->renderer->houseKeep();

  delete app->vulkanManager;
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);

  glfwTerminate();

  return 0;
}
