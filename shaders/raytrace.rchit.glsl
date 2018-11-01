#version 460
#extension GL_NVX_raytracing : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_nonuniform_qualifier : require
//#extension GL_KHX_shader_explicit_arithmetic_types : require
//#extension GL_KHX_shader_explicit_arithmetic_types_int8 : require

struct TriangleData
{
  float n0x, n0y, n0z;
  float n1x, n1y, n1z;
  float n2x, n2y, n2z;
  float r, g, b;
};

struct Payload
{
  vec3 color;
  uint state;
};

layout(location = 0) rayPayloadInNVX Payload payloadIn;
layout(location = 1) rayPayloadNVX Payload payload;
layout(location = 2) hitAttributeNVX vec3 hitAttribute;

layout(binding = 0) uniform accelerationStructureNVX topLevel;
layout(std140, binding = 2) uniform SceneBuf{
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
} sceneBuf;

layout(std140, binding = 3) buffer TriangleDataBuffer {
    TriangleData data[];
} triangles[];

float rand(inout uint state)
{
  state = 1664525 * state + 1013904223;
  return (1.0 / 4294967296.0)*state;
}

const float two_pi = float(2.0 * 3.14159265358979323846264338327950288);

vec3 randomCosineDir(uint state)
{
  float r1 = min(1,rand(state));
  float r2 = min(1,rand(state));
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

void main() {

  TriangleData data = triangles[nonuniformEXT(gl_InstanceID)].data[gl_PrimitiveID];

  float w0 = hitAttribute.x;
  float w1 = hitAttribute.y;
  float w2 = 1.f - w0 - w1;
  vec3 n = w2 * vec3(data.n0x, data.n0y, data.n0z) +
           w0 * vec3(data.n1x, data.n1y, data.n1z) +
           w1 * vec3(data.n2x, data.n2y, data.n2z);

  vec3 o = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;
  vec3 d = normalize(vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz));

  vec3 u, v, w;
  orthonormal(u, v, w, n);

  uint state = payloadIn.state;
  vec3 d_l = randomCosineDir(state);
  vec3 d_w = normalize(d_l.x*u + d_l.y*v + d_l.z*w);

  //d_w = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);

  payload.state = state;
  traceNVX(topLevel,
           gl_RayFlagsOpaqueNVX | gl_RayFlagsCullBackFacingTrianglesNVX,   // rayFlags    gl_RayFlagsTerminateOnFirstHitNVX;
           ~0u,                    // cullMask
           0,                      // stbRecordOffset
           0,                      // stbRecordStride
           0,                      // missIndex
           o,                      // origin
           0.001f,                 // tMin
           d_w,                    // dir
           1000.f,                 // tMax
           1);                     // Payload

  payloadIn.color = 0.2f*payload.color;

  //vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);
  //colorIn =  0.3f*max(0.5f,dot(n,d))*color;// irradiance(n);
}