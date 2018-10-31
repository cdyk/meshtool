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

  //TriangleData data = triangles[nonuniformEXT(gl_InstanceID)].data[0];
  //TriangleData data = triangles[0].data[0];

  color = vec3(1.f-hitAttribute.x - hitAttribute.y, hitAttribute.xy);
  //color.r = data.r;
  //color.g = data.g;
  //color.b = data.b;
}