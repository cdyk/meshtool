#pragma once
#include "LinAlg.h"

class Viewer
{
public:
  Viewer();

  void setViewVolume(const BBox3f& viewVolume);
  void viewAll();

  void view(const BBox3f& box);

  void update();

  void resize(const Vec2f& pos, const Vec2f& size);
  void startRotation(float x, float y);
  void startPan(float x, float y);
  void startZoom(float x, float y);
  void stopAction();

  void move(float x, float y);

  void dolly(float x, float y, float speed, bool distance=false);

  const Mat4f& getProjectionMatrix() const { return curr.P; }
  const Mat4f& getViewMatrix() const { return curr.M; }
  const Mat4f& getViewInverseMatrix() const { return curr.Minv; }
  const Mat4f& getProjectionViewMatrix() const { return curr.PM; }
  const Mat4f& getProjectionViewInverseMatrix() const { return curr.PMinv; }

private:
  enum Projection {
    Perspective,
    Orthographic
  } projection = Projection::Perspective;

  enum Mode {
    None,
    Rotation,
    Pan,
    Zoom
  } mode = Mode::None;

  struct CamState
  {
    Mat4f P;
    Mat4f M;
    Mat4f PM;
    Mat4f Pinv;
    Mat4f Minv;
    Mat4f PMinv;
    Quatf orientation;
    Vec3f coi;
    Vec2f mouse2;
    Vec3f mouse3;
    float dist;
  };
  CamState init;
  CamState curr;
  Vec2f winPos;
  Vec2f winSize;
  BBox3f viewVolume;
  float fov = 1.f;            // fov along y
  float viewVolumeRadius;
  bool needsUpdate = true;
};