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

layout(location = 0) rayPayloadInNVX vec3 color;
layout(location = 1) hitAttributeNVX vec3 hitAttribute;

layout(std140, binding = 2) uniform SceneBuf{
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
} sceneBuf;

layout(std140, binding = 3) buffer TriangleDataBuffer {
    TriangleData data[];
} triangles[];

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

  vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);

  color = 0.3f*irradiance(n);
}