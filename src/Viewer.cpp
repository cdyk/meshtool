#include <cstdio>
#include <cmath>

#include "Viewer.h"
#include "LinAlgOps.h"

Viewer::Viewer() :
  winSize(1,1),
  P(createIdentityMat4f()),
  M(createIdentityMat4f())
{
  setViewVolume(BBox3f(Vec3f(-1.f), Vec3f(1.f)));
  viewAll();
}

void Viewer::setViewVolume(const BBox3f& box)
{
  viewVolume = box;
  coi = 0.5f*(box.min + box.max);
  viewVolumeRadius = 0.5f * distance(viewVolume.min, viewVolume.max);
}

void Viewer::viewAll()
{
  coi = 0.5f * (viewVolume.min + viewVolume.max);
  dist = viewVolumeRadius / std::tan(0.5f*fov);
}


void Viewer::update()
{
  auto aspect = winSize.x / winSize.y;
  auto epsilon = 0.001f * viewVolumeRadius;

  auto near = 0.1f;
  auto far = 100.f;

  if (projection == Projection::Perspective) {
    auto f = 1.f / std::tan(0.5f*fov);
    auto g = f / aspect;
    auto h = (far + near) / (near - far);
    auto j = (2 * far*near) / (near - far);
    P = Mat4f(  g, 0.f,  0.f, 0.f,
              0.f,   f,  0.f, 0.f,
              0.f, 0.f,    h,   j,
              0.f, 0.f, -1.f, 0.f);
  }
  else if (projection == Projection::Orthographic) {
    auto h = std::tan(0.5f*fov) *dist;
    auto w = aspect * h;

    auto tx = 0.f / (2*w);
    auto ty = 0.f /(2*h);
    auto tz = -(far + near) / (far - near);
    auto sx = 1.f / w;
    auto sy = -1.f / h;
    auto sz = -2.f / (far - near);
    P = Mat4f( sx, 0.f, 0.f,  tx,
              0.f,  sy, 0.f,  ty,
              0.f, 0.f,  sz,  tz,
              0.f, 0.f, 0.f, 1.f);
  }

  M = mul(translationMatrix(Vec3f(0, 0, -dist)), mul(Mat4f(rotationMatrix(orientation)), translationMatrix(Vec3f(-coi.x, -coi.y, -coi.z))));
}

void Viewer::resize(int w, int h)
{
  winSize.x = float(w < 1 ? 1 : w);
  winSize.y = float(h < 1 ? 1 : h);

  fprintf(stderr, "resize: x=%d, y=%d\n", w, h);
}

void Viewer::startRotation(float x, float y)
{
  stopAction();
  state = State::Rotation;
  fprintf(stderr, "startRotation: x=%f, y=%f\n", x, y);
}

void Viewer::startPan(float x, float y)
{
  stopAction();
  state = State::Pan;
  fprintf(stderr, "startPan: x=%f, y=%f\n", x, y);
}

void Viewer::startZoom(float x, float y)
{
  stopAction();
  state = State::Zoom;
  fprintf(stderr, "startZoom: x=%f, y=%f\n", x, y);
}

void Viewer::dolly(float x, float y)
{
  stopAction();
  fprintf(stderr, "dolly: x=%f, y=%f\n", x, y);
}

void Viewer::stopAction()
{
  if (state == State::None) return;
  state = State::None;
  fprintf(stderr, "stopAction\n");
}

void Viewer::move(float x, float y)
{
  if (state == State::None) return;

  fprintf(stderr, "move: x=%f, y=%f\n", x, y);
}
