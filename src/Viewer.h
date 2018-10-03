#pragma once
#include "LinAlg.h"

class Viewer
{
public:
  Viewer();

  void setViewVolume(const BBox3f& viewVolume);
  void viewAll();


  void update();

  void resize(int w, int h);
  void startRotation(float x, float y);
  void startPan(float x, float y);
  void startZoom(float x, float y);
  void stopAction();

  void move(float x, float y);

  void dolly(float x, float y);

  const Mat4f& getProjectionMatrix() const { return curr.P; }
  const Mat4f& getViewMatrix() const { return curr.M; }

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
    Vec2f mouse;
    Vec3f mouse3;
  };
  CamState init;
  CamState curr;

  Vec2f winSize;
  BBox3f viewVolume;
  float fov = 1.f;            // fov along y
  float dist;
  float viewVolumeRadius;

  Vec3f position;
};