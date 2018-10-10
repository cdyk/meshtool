#include "App.h"
#include "Viewer.h"

App::App()
{
  viewer = new Viewer();
}

App::~App()
{
  delete viewer;
}
