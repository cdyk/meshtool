#include <cstdio>
#include <cmath>
#include <cassert>

#include "Viewer.h"
#include "LinAlgOps.h"


namespace {


  Vec3f getPointOnUnitSphere(Vec2f p)
  {
    auto r2 = lengthSquared(p);
    if (r2 < 0.25f) {
      // on sphere
      return Vec3f(2.f*p.x, 2.f*p.y, std::sqrt(1.f - 4.f*r2));
    }
    else {
      // outside sphere
      auto r = 1.f / std::sqrt(r2);
      return Vec3f(r*p.x, r*p.y, 0.f);
    }
  }


  bool trackball(Vec3f& axis, float& angle, const Vec2f& winSize, const Vec2f& p0, const Vec2f& p1)
  {
    auto aspect = winSize.x / winSize.y;
    auto s0 = getPointOnUnitSphere(Vec2f((p1.x / winSize.x - 0.5f)*aspect, -p1.y / winSize.y + 0.5f));
    auto s1 = getPointOnUnitSphere(Vec2f((p0.x / winSize.x - 0.5f)*aspect, -p0.y / winSize.y + 0.5f));
    axis = normalize(cross(s0, s1));
    if (std::isfinite(axis.x)) {
      angle = std::acos(dot(s0, s1));
      return true;
    }
    else {
      return false;
    }
  }

  Vec3f getPointOnInterestPlane(const Mat4f& PM, const Mat4f& PMinv, const Vec3f& coi, const Vec2f& winSize, const Vec2f& p0)
  {
    auto hcoi = mul(PM, Vec4f(coi, 1.f));
    auto clipDepth = hcoi.z / hcoi.w;

    auto hp = mul(PMinv, Vec4f(2.f*p0.x / winSize.x - 1.f,
                               -2.f*p0.y / winSize.y - 1.f,
                               clipDepth,
                               1.f));
    return (1.f / hp.w)*Vec3f(hp.x, hp.y, hp.z);
  }


}


Viewer::Viewer() :
  winSize(1,1)
{
  curr.P = createIdentityMat4f();
  curr.M = createIdentityMat4f();
  curr.orientation = creatUnitQuatf();

  setViewVolume(BBox3f(Vec3f(-1.f), Vec3f(1.f)));
  viewAll();
  needsUpdate = true;
}

void Viewer::setViewVolume(const BBox3f& box)
{
  stopAction();
  viewVolume = box;
  curr.coi = 0.5f*(box.min + box.max);
  viewVolumeRadius = 0.5f * distance(viewVolume.min, viewVolume.max);
  needsUpdate = true;
}

void Viewer::viewAll()
{
  stopAction();
  curr.coi = 0.5f * (viewVolume.min + viewVolume.max);
  curr.dist = viewVolumeRadius / std::tan(0.5f*fov);
  needsUpdate = true;
}


void Viewer::update()
{
  if (needsUpdate == false) return;
  needsUpdate = false;

  auto aspect = winSize.x / winSize.y;
  auto epsilon = 0.001f * viewVolumeRadius;

  auto m0 = translationMatrix(Vec3f(0, 0, -curr.dist));
  auto m1 = Mat4f(rotationMatrix(curr.orientation));
  auto m2 = translationMatrix(Vec3f(-curr.coi.x, -curr.coi.y, -curr.coi.z));
  curr.M = mul(m0, mul(m1, m2));

  auto r0 = translationMatrix(Vec3f(0, 0, curr.dist));
  auto r1 = Mat4f(rotationMatrix(conjugate(curr.orientation)));
  auto r2 = translationMatrix(Vec3f(curr.coi.x, curr.coi.y, curr.coi.z));
  curr.Minv = mul(r2, mul(r1, r0));

  auto near = FLT_MAX;
  auto far = -FLT_MAX;
  for (unsigned i = 0; i < 8; i++) {
    auto ph = mul(curr.M, Vec4f((i & 1) ? viewVolume.min.x : viewVolume.max.x,
                                (i & 2) ? viewVolume.min.y : viewVolume.max.y,
                                (i & 4) ? viewVolume.min.z : viewVolume.max.z,
                                1.f));
    auto z = -ph.z / ph.w;
    near = z < near ? z : near;
    far = far < z ? z : far;
  }


  if (projection == Projection::Perspective) {

    if (near < 0.001f*viewVolumeRadius) near = 0.001f*viewVolumeRadius;
    if (far < near + 0.01f*viewVolumeRadius) far = near + 0.01f*viewVolumeRadius;

    auto b = 1.f / std::tan(0.5f*fov);
    auto a = b / aspect;
    auto c = (far + near) / (near - far);
    auto d = (2 * far*near) / (near - far);
    curr.P = Mat4f(  a, 0.f,  0.f, 0.f,
                   0.f,   b,  0.f, 0.f,
                   0.f, 0.f,    c,   d,
                   0.f, 0.f, -1.f, 0.f);
    curr.Pinv = Mat4f(1.f/a,     0,     0,    0,
                          0, 1.f/b,     0,    0,
                          0,     0,     0, -1.f,
                          0,     0, 1.f/d,  c/d);
  }
  else if (projection == Projection::Orthographic) {
    auto h = std::tan(0.5f*fov) * curr.dist;
    auto w = aspect * h;

    auto tx = 0.f / (2*w);
    auto ty = 0.f /(2*h);
    auto tz = -(far + near) / (far - near);
    auto sx = 1.f / w;
    auto sy = -1.f / h;
    auto sz = -2.f / (far - near);

    curr.P = Mat4f( sx, 0.f, 0.f,  tx,
                   0.f,  sy, 0.f,  ty,
                   0.f, 0.f,  sz,  tz,
                   0.f, 0.f, 0.f, 1.f);
  }


  curr.PM = mul(curr.P, curr.M);
  curr.PMinv = mul(curr.Minv, curr.Pinv);

  //auto g = mul(curr.PM, curr.PMinv);
  //for (unsigned j = 0; j < 4; j++) {
  //  for (unsigned i = 0; i < 4; i++) {
  //    assert(std::abs(g.data[4 * j + i] - (i == j ? 1.f : 0.f)) < 1e-3f);
  //  }
  //}

}

void Viewer::resize(int w, int h)
{
  stopAction();
  winSize.x = float(w < 1 ? 1 : w);
  winSize.y = float(h < 1 ? 1 : h);
  needsUpdate = true;
}

void Viewer::startRotation(float x, float y)
{
  stopAction();
  curr.mouse2 = Vec2f(x, y);
  init = curr;
  mode = Mode::Rotation;
}

void Viewer::startPan(float x, float y)
{
  stopAction();
  curr.mouse3 = getPointOnInterestPlane(curr.PM, curr.PMinv, curr.coi, winSize, Vec2f(x, y));
  init = curr;
  mode = Mode::Pan;
}

void Viewer::startZoom(float x, float y)
{
  stopAction();
  curr.mouse2 = Vec2f(x, y);
  curr.mouse3 = getPointOnInterestPlane(curr.PM, curr.PMinv, curr.coi, winSize, Vec2f(x, y));
  init = curr;
  mode = Mode::Zoom;
}

void Viewer::dolly(float x, float y)
{
  stopAction();

  auto a = getPointOnInterestPlane(curr.PM, curr.PMinv, curr.coi, winSize, Vec2f(0, 0.5f*winSize.y));
  auto b = getPointOnInterestPlane(curr.PM, curr.PMinv, curr.coi, winSize, Vec2f(winSize.x, 0.5f*winSize.y));
  auto l = length(b - a);

  auto zh = mul(curr.Minv, Vec4f(0, 0, 1, 0));
  auto z = normalize(Vec3f(zh.x, zh.y, zh.z));

  curr.coi = curr.coi + 0.05f*y*l*z;
  needsUpdate = true;
}

void Viewer::stopAction()
{
  if (mode == Mode::None) return;
  mode = Mode::None;
}

void Viewer::move(float x, float y)
{
  curr.mouse2 = Vec2f(x, y);

  switch (mode) {
  case  Mode::Rotation: {
    Vec3f axis;
    float angle;
    if (trackball(axis, angle, winSize, init.mouse2, curr.mouse2)) {
      curr.orientation = mul(init.orientation, axisAngleRotation(axis, 2.f*angle));
    }
    needsUpdate = true;
    break;
  }
  case Mode::Pan:
    curr.mouse3 = getPointOnInterestPlane(init.PM, init.PMinv, init.coi, winSize, Vec2f(x, y));
    curr.coi = init.coi - (curr.mouse3 - init.mouse3);
    needsUpdate = true;
    break;
  case Mode::Zoom:
    curr.mouse2 = Vec2f(x, y);
    curr.mouse3 = getPointOnInterestPlane(init.PM, init.PMinv, init.coi, winSize, Vec2f(x, y));
    curr.dist = init.dist + std::copysign(2.f*length(curr.mouse3 - init.mouse3), init.mouse2.y-curr.mouse2.y);
    if (curr.dist < 0.01f*viewVolumeRadius) curr.dist = 0.01f*viewVolumeRadius;
    needsUpdate = true;
    break;
  }

}
