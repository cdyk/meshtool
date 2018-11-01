#include "raytrace.binding.h"

layout(binding = BINDING_TOPLEVEL_ACC) uniform accelerationStructureNVX topLevel;

layout(binding = BINDING_OUTPUT_IMAGE, rgba32f) uniform writeonly image2D image;
layout(binding = BINDING_INPUT_IMAGE, rgba32f) uniform readonly image2D imageIn;

layout(std140, binding = BINDING_SCENE_BUF) uniform SceneBuf{
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
  uint rndState;
} sceneBuf;

struct TriangleData
{
  float n0x, n0y, n0z;
  float n1x, n1y, n1z;
  float n2x, n2y, n2z;
  float r, g, b;
};
layout(std140, binding = BINDING_TRIANGLE_DATA) buffer TriangleDataBuffer {
  TriangleData data[];
} triangles[];


struct Payload
{
  vec3 color;
  uint state;
};



uint xorShift(uint state)
{
  state ^= (state << 13);
  state ^= (state >> 17);
  state ^= (state << 5);
  return state;
}

uint wangHash(uint state)
{
  state = (state ^ 61) ^ (state >> 16);
  state *= 9;
  state = state ^ (state >> 4);
  state *= 0x27d4eb2d;
  state = state ^ (state >> 15);
  return state;
}

float rand(inout uint state)
{
  state = 1664525 * state + 1013904223;
  return (1.0 / 4294967296.0)*state;
}

const float two_pi = float(2.0 * 3.14159265358979323846264338327950288);

vec3 randomCosineDir(uint state)
{
  float r1 = min(1, rand(state));
  float r2 = min(1, rand(state));
  float z = sqrt(1.f - r2);
  float phi = two_pi * r1;
  float two_sqrt_r2 = sqrt(r2);
  float x = two_sqrt_r2 * cos(phi);
  float y = two_sqrt_r2 * sin(phi);
  return vec3(x, y, z);
}

void orthonormal(out vec3 u, out vec3 v, out vec3 w, in vec3 d)
{
  w = normalize(d);
  vec3 a = 0.9f < abs(w.x) ? vec3(0, 1, 0) : vec3(1, 0, 0);
  v = normalize(cross(w, a));
  u = cross(w, v);
}

vec3 irradiance(vec3 d)
{
  vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);
  vec3 u = vec3(sceneBuf.ux, sceneBuf.uy, sceneBuf.uz);
  return clamp(0.5f + dot(u, d), 0, 1) * vec3(0, 0.5, 1) + max(0.f, dot(l, d)) * vec3(7, 6, 5);
}
