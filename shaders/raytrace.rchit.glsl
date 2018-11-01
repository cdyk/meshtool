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

layout(std140, binding = 3) buffer TriangleDataBuffer {
    TriangleData data[];
} triangles[];

void main() {

  TriangleData data = triangles[nonuniformEXT(gl_InstanceID)].data[gl_PrimitiveID];

  float w0 = hitAttribute.x;
  float w1 = hitAttribute.y;
  float w2 = 1.f - w0 - w1;
  vec3 n = w2 * vec3(data.n0x, data.n0y, data.n0z) +
           w0 * vec3(data.n1x, data.n1y, data.n1z) +
           w1 * vec3(data.n2x, data.n2y, data.n2z);

  color = abs(n);
}