#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
//#include <GL/gl3w.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <list>
#include <cctype>
#include <chrono>
#include <mutex>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl2.h>

#include <GLFW/glfw3.h>
#include <cstdio>
#include "Viewer.h"
#include "Common.h"
#include "Mesh.h"
#include "LinAlgOps.h"


namespace {

  Viewer viewer;
  Tasks tasks;
  int width, height;
  float leftSplit = 100;
  float thickness = 8;

  bool solid = true;

  std::mutex incomingMeshLock;
  std::list<Mesh*> incomingMeshes;

  std::list<Mesh*> meshes;

  void glfw_error_callback(int error, const char* description)
  {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
  }

  void resizeFunc(GLFWwindow* window, int w, int h)
  {
  }

  void moveFunc(GLFWwindow* window, double x, double y)
  {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    viewer.move(float(x), float(y));
  }

  void buttonFunc(GLFWwindow* window, int button, int action, int mods)
  {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    switch (action) {
    case GLFW_PRESS:
      switch (button) {
      case 0: viewer.startRotation(float(x), float(y)); break;
      case 1: viewer.startZoom(float(x), float(y)); break;
      case 2: viewer.startPan(float(x), float(y)); break;
      }
      break;
    case GLFW_RELEASE:
      viewer.stopAction();
      break;
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
    logger(0, "Failed to read %s", path.c_str());
  }

  

}



int main(int argc, char** argv)
{
  GLFWwindow* window;
  tasks.init(logger);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return -1;

  window = glfwCreateWindow(1280, 720, "Hello World", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  glfwSetWindowSizeCallback(window, resizeFunc);
  glfwSetCursorPosCallback(window, moveFunc);
  glfwSetMouseButtonCallback(window, buttonFunc);
  glfwSetScrollCallback(window, scrollFunc);
  glfwMakeContextCurrent(window);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  ImGui_ImplGlfw_InitForOpenGL(window, false);
  ImGui_ImplOpenGL2_Init();

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


  glfwGetFramebufferSize(window, &width, &height);
  leftSplit = 0.25f*width;

  while (!glfwWindowShouldClose(window))
  {
    tasks.update();

    bool change = false;
    {
      std::lock_guard<std::mutex> guard(incomingMeshLock);
      if (!incomingMeshes.empty()) {
        for (auto &m : incomingMeshes) {
          meshes.push_back(m);
        }
        incomingMeshes.clear();
        change = true;
      }
    }
    if (change) {
      auto bbox = createEmptyBBox3f();
      for (auto * m : meshes) {
        if (isEmpty(m->bbox)) continue;
        engulf(bbox, m->bbox);
      }
      if (isEmpty(bbox)) {
        bbox = BBox3f(Vec3f(-1.f), Vec3f(1.f));
      }
      viewer.setViewVolume(bbox);
      viewer.viewAll();
    }

    glfwGetFramebufferSize(window, &width, &height);

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    float menuHeight = 0.f;
    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", nullptr, nullptr)) { glfwSetWindowShouldClose(window, true); }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("View all", nullptr, nullptr)) { viewer.viewAll(); }
        if (ImGui::MenuItem("Solid", nullptr, &solid)) {  }
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
      for (auto &m : meshes) {
        if (ImGui::TreeNodeEx(m, ImGuiTreeNodeFlags_DefaultOpen, "%s Vn=%d Tn=%d", m->name ? m->name : "unnamed", m->vtx_n, m->tri_n)) {
          for (unsigned o = 0; o < m->obj_n; o++) {
            if (ImGui::Button(m->obj[o])) {

            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::EndChild();
      ImGui::End();
    }
    //ImGui::ShowDemoWindow();

    ImGui::Render();


    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    {
      Vec2f viewerPos(leftSplit, menuHeight);
      Vec2f viewerSize(width - viewerPos.x, height - viewerPos.y);
      glViewport(int(viewerPos.x), int(viewerPos.y), int(viewerSize.x), int(viewerSize.y));
      viewer.resize(viewerPos, viewerSize);

      glEnable(GL_DEPTH_TEST);

      viewer.update();

      glMatrixMode(GL_PROJECTION);
      glLoadMatrixf(viewer.getProjectionMatrix().data);


      glMatrixMode(GL_MODELVIEW);
      glLoadMatrixf(viewer.getViewMatrix().data);

      if (solid) {
        glPolygonOffset(1.f, 1.f);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glColor3f(0.2f, 0.2f, 0.6f);
        for (auto * m : meshes) {
          glBegin(GL_TRIANGLES);
          for (unsigned i = 0; i < 3 * m->tri_n; i++) {
            glVertex3fv(m->vtx[m->tri[i]].data);
          }
          glEnd();
        }
        glDisable(GL_POLYGON_OFFSET_FILL);
      }

      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glColor3f(1.f, 1.f, 1.f);
      for (auto * m : meshes) {
        glBegin(GL_TRIANGLES);
        for (unsigned i = 0; i < 3 * m->tri_n; i++) {
          glVertex3fv(m->vtx[m->tri[i]].data);
        }
        glEnd();
      }
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }


    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);

  glfwTerminate();

  return 0;
}
