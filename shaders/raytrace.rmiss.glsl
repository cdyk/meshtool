#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 color;

void main() {
  color = abs(gl_WorldRayDirectionNVX);
}
