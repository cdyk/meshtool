#include <GLFW/glfw3.h>
#include <cstdio>
#include "Viewer.h"


namespace {

  Viewer viewer;


  void resizeFunc(GLFWwindow* window, int w, int h)
  {
    viewer.resize(w, h);
  }

  void moveFunc(GLFWwindow* window, double x, double y)
  {
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

  
  BBox3f viewVolume(Vec3f(-2.f), Vec3f(2.f));
  viewer.setViewVolume(viewVolume);
  viewer.resize(1280, 720);

  while (!glfwWindowShouldClose(window))
  {
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    viewer.update();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(viewer.getProjectionMatrix().data);


    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(viewer.getViewMatrix().data);


    glBegin(GL_QUADS);
    glVertex3f(-1, -1, -1);
    glVertex3f( 1, -1, -1);
    glVertex3f( 1,  1, -1);
    glVertex3f(-1,  1, -1);
    glEnd();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();

  return 0;
}
