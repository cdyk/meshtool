#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 color;

layout(std140, binding = 2) uniform SceneBuf{
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
} sceneBuf;

vec3 irradiance(vec3 d)
{
  vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);
  vec3 u = vec3(sceneBuf.ux, sceneBuf.uy, sceneBuf.uz);
  return clamp(0.5f + dot(u, d), 0, 1) * vec3(0, 0.5, 1) + max(0.f, dot(l, d)) * vec3(7, 6, 5);
}

void main() {


  color = irradiance(gl_WorldRayDirectionNVX);
}
