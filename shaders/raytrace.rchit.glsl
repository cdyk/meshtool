#version 460
#extension GL_NVX_raytracing : require
#extension GL_EXT_shader_8bit_storage : require

struct TriangleData
{
  uint8_t n0x, n0y, n0z;
  uint8_t n1x, n1y, n1z;
  uint8_t n2x, n2y, n2z;
  uint8_t r, g, b;
};

layout(location = 0) rayPayloadInNVX vec3 color;
layout(location = 1) hitAttributeNVX vec3 hitAttribute;
layout(binding = 2) uniform TriangleDataBuffer
{
    TriangleData data[];
} triangles[];

void main() {
  color = vec3(1.f-hitAttribute.x - hitAttribute.y, hitAttribute.xy);
  color.r = (gl_PrimitiveID & 0xf)*(1.f / 15.f);
  color.g = 0.f;
  color.b = 0.f;
}