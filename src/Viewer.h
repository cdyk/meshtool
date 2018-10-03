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

  const Mat4f& getProjectionMatrix() const { return P; }
  const Mat4f& getViewMatrix() const { return M; }

private:
  enum Projection {
    Perspective,
    Orthographic
  } projection = Projection::Perspective;

  enum State {
    None,
    Rotation,
    Pan,
    Zoom
  } state = State::None;

  Vec2f winSize;
  BBox3f viewVolume;
  Quatf orientation;
  Vec3f coi;
  float fov = 1.f;            // fov along y
  float dist;
  float viewVolumeRadius;

  Vec3f position;
  Mat4f P;              // Current projection matrix,
  Mat4f M;              // Current view matrix.
};