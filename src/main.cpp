#define NOMINMAX
//#include <GL/gl3w.h>
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_opengl2.h>

#include <GLFW/glfw3.h>
#include <cstdio>
#include "Viewer.h"


namespace {

  Viewer viewer;


  void glfw_error_callback(int error, const char* description)
  {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
  }

  void resizeFunc(GLFWwindow* window, int w, int h)
  {
    viewer.resize(w, h);
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
    viewer.dolly(float(x), float(y));
  }

}



int main(int argc, char** argv)
{
  GLFWwindow* window;

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

  while (!glfwWindowShouldClose(window))
  {

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar())
    {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Quit", nullptr, nullptr)) { glfwSetWindowShouldClose(window, true); }
        ImGui::EndMenu();
      }


      ImGui::EndMainMenuBar();
    }



    ImVec2 mainPos;
    ImVec2 mainSize;

    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

      ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

      ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f    

      if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }


    ImGui::Render();


    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    viewer.update();

#if 1
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(viewer.getProjectionMatrix().data);


    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(viewer.getViewMatrix().data);


    glBegin(GL_QUADS);
    glColor3f(1, 0, 0);
    glVertex3f(-1, -1, -1);
    glVertex3f( 1, -1, -1);
    glVertex3f( 1,  1, -1);
    glVertex3f(-1,  1, -1);
    glColor3f(0, 1, 0);
    glVertex3f(-1, -1, 1);
    glVertex3f(1, -1, 1);
    glVertex3f(1, 1, 1);
    glVertex3f(-1, 1, 1);
    glEnd();
#endif

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
