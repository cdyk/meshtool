#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
//#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <list>
#include <vector>
#include <cctype>
#include <chrono>
#include <mutex>
#include <examples/imgui_impl_glfw.h>

#include <cstdio>
#include "Viewer.h"
#include "Common.h"
#include "Mesh.h"
#include "LinAlgOps.h"
#include "Renderer.h"
#include "VulkanMain.h"


namespace {

  Viewer viewer;
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



  Renderer* renderer = nullptr;

  std::mutex incomingMeshLock;
  std::list<Mesh*> incomingMeshes;

  Vector<RenderMesh*> renderMeshes;

  struct MeshItem
  {
    Mesh* mesh;
    RenderMesh* renderMesh;
  };

  std::list<MeshItem> meshItems;

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
    wasResized = true;
    width = w;
    height = h;
  }

  void moveFunc(GLFWwindow* window, double x, double y)
  {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    viewer.move(float(x), float(y));
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
          picking = true;
        }
        else {
          viewer.startRotation(float(x), float(y));
        }
        break;
      case 1: viewer.startZoom(float(x), float(y)); break;
      case 2: viewer.startPan(float(x), float(y)); break;
      }
      break;
    case GLFW_RELEASE:
      viewer.stopAction();
      break;
    }
  }

  void keyFunc(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    if (mods == GLFW_MOD_CONTROL) {
      if (key == GLFW_KEY_A && action == GLFW_PRESS) selectAll = true;
      if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) selectNone = true;
    }
    else if (mods == (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT)) {
      if (key == GLFW_KEY_A && action == GLFW_PRESS) viewAll = true;
      if (key == GLFW_KEY_S && action == GLFW_PRESS) moveToSelection = true;
    }
    else if (mods == 0) {
      if (key == GLFW_KEY_W && action == GLFW_PRESS)  renderer->outlines = !renderer->outlines;
      if (key == GLFW_KEY_S && action == GLFW_PRESS)  renderer->solid = !renderer->solid;
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

    viewer.dolly(float(x), float(y), speed, distance);
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
      std::lock_guard<std::mutex> guard(incomingMeshLock);
      incomingMeshes.push_back(mesh);
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
          selectAll = true;
        }
        if (ImGui::MenuItem("None", "CTRL+ ")) {
          selectNone = true;
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("View all", "CTRL+SHIFT+A", nullptr)) { viewAll = true; }
        if (ImGui::MenuItem("View selection", "CTRL+SHIFT+S")) { moveToSelection = true; }
        ImGui::Separator();
        if (ImGui::MenuItem("Solid", "S", &renderer->solid)) {}
        if (ImGui::MenuItem("Outlines", "W", &renderer->outlines)) {}
        ImGui::EndMenu();
      }
      menuHeight = ImGui::GetWindowSize().y;
      ImGui::EndMainMenuBar();
    }


    {
      ImGui::SetNextWindowPos(ImVec2(0, menuHeight));
      ImGui::SetNextWindowSize(ImVec2(leftSplit, height - menuHeight));
      ImGui::Begin("##left", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);


      //ImGuiContext& g = *GImGui;
      //ImGuiWindow* window = g.CurrentWindow;

      ImVec2 backup_pos = ImGui::GetCursorPos();
      ImGui::SetCursorPosX(leftSplit - thickness);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.10f));
      ImGui::Button("##Splitter", ImVec2(thickness, -1));

      ImGui::PopStyleColor(3);
      if (ImGui::IsItemActive()) {
        leftSplit += ImGui::GetIO().MouseDelta.x;
        if (leftSplit < 50) leftSplit = 50;
        if (width - thickness < leftSplit) leftSplit = width - thickness;
      }

      ImGui::SetCursorPos(backup_pos);
      ImGui::BeginChild("Meshes", ImVec2(leftSplit - thickness - backup_pos.x, -1), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar);
      for(auto & item : meshItems) {
        auto * m = item.mesh;
        if (ImGui::TreeNodeEx(m, ImGuiTreeNodeFlags_DefaultOpen, "%s Vn=%d Tn=%d", m->name ? m->name : "unnamed", m->vtxCount, m->triCount)) {
          for (unsigned o = 0; o < m->obj_n; o++) {
            if (ImGui::Selectable(m->obj[o], &m->selected[o], ImGuiSelectableFlags_PressedOnClick)) {
              updateColor = true;
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
      std::lock_guard<std::mutex> guard(incomingMeshLock);
      if (!incomingMeshes.empty()) {
        newMeshes = std::move(incomingMeshes);
        incomingMeshes.clear();
      }
    }
    if (!newMeshes.empty()) {
      for (auto & m : newMeshes) {

        m->currentColor = (uint32_t*)m->arena.alloc(sizeof(uint32_t) * m->triCount);
        m->selected = (bool*)m->arena.alloc(sizeof(bool)*m->obj_n);
        std::memset(m->selected, 0, sizeof(bool)*m->obj_n);
        updateColor = true;

        meshItems.push_back({ m, renderer->createRenderMesh(m) });
      }
      auto bbox = createEmptyBBox3f();
      for (auto & item : meshItems) {
        auto * m = item.mesh;
        if (isEmpty(m->bbox)) continue;
        engulf(bbox, m->bbox);
      }
      if (isEmpty(bbox)) {
        bbox = BBox3f(Vec3f(-1.f), Vec3f(1.f));
      }
      viewer.setViewVolume(bbox);
      viewer.viewAll();
    }
  }

}



int main(int argc, char** argv)
{
  GLFWwindow* window;
  tasks.init(logger);

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
  glfwGetFramebufferSize(window, &width, &height);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  renderer = main_VulkanInit(logger, window, width, height);


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
  viewer.setViewVolume(viewVolume);
  viewer.resize(1280, 720);



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
        tasks.enqueue(taskFunc);
      }
    }
  }

  leftSplit = 0.25f*width;
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();
    if (wasResized) {
      wasResized = false;
      main_VulkanResize(width, height);
    }
    {
      Vec2f viewerPos(leftSplit, menuHeight);
      Vec2f viewerSize(width - viewerPos.x, height - viewerPos.y);
      viewer.resize(viewerPos, viewerSize);
      viewer.update();
    }

    tasks.update();
    checkQueues();

    main_VulkanStartFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    guiTraversal(window);
    ImGui::Render();

    Vec4f viewport(leftSplit, menuHeight, width - leftSplit, height - menuHeight);

    if (picking) {
      picking = false;
      double x, y;
      glfwGetCursorPos(window, &x, &y);

      logger(0, "picking at %f %f", x, y);
    }

    if (selectAll | selectNone) {
      selectAll = selectNone = false;
      for (auto & m : meshItems) {
        for (uint32_t i = 0; i < m.mesh->obj_n; i++) {
          m.mesh->selected[i] = selectAll;
          updateColor = true;
        }
      }
    }

    if (viewAll) {
      viewAll = false;
      viewer.viewAll();
    }

    if (moveToSelection) {
      moveToSelection = false;
      BBox3f bbox = createEmptyBBox3f();
      for (auto & it : meshItems) {
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
        viewer.view(bbox);
      }
    }

    if (updateColor) {
      unsigned k = 0;
      for (auto & it : meshItems) {
        auto * m = it.mesh;
        for (uint32_t i = 0; i < m->triCount; i++) {
          if (m->selected[m->TriObjIx[i]]) {
            m->currentColor[i] = 0xffddffff;
            k++;
          }
          else {
            m->currentColor[i] = 0xffff8888;
          }
        }
      }
      logger(0, "Highlighted %d triangles", k);
    }

    renderMeshes.resize(meshItems.size());
    size_t i = 0; 
    for (auto & it : meshItems) {
      renderMeshes[i++] = it.renderMesh;
      if (updateColor) {
        renderer->updateRenderMeshColor(it.renderMesh);
      }
    }
    updateColor = false;

    main_VulkanRender(width, height, renderMeshes, viewport, viewer.getProjectionMatrix(), viewer.getViewMatrix());
    main_VulkanPresent();
  }

  for (auto & item : meshItems) {
    renderer->destroyRenderMesh(item.renderMesh);
  }
  meshItems.clear();

  main_VulkanCleanup();

  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);

  glfwTerminate();

  return 0;
}
