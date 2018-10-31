#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 color;
layout(location = 1) hitAttributeNVX vec3 hitAttribute;

void main() {
  color = vec3(1.f-hitAttribute.x - hitAttribute.y, hitAttribute.xy);
}