#version 460
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV vec3 color;

layout(std140, binding = 2) uniform SceneBuf{
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
} sceneBuf;

vec3 irradiance(vec3 d)
{
  vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);
  vec3 u = vec3(sceneBuf.ux, sceneBuf.uy, sceneBuf.uz);

  vec3 sky = clamp(0.5f + dot(u, d), 0, 1) * vec3(0, 0.5, 1) * 0.2;
  vec3 low = pow(max(0,dot(-u,d)),5) * vec3(1.0, 0.4, 0.1) * 1;
  vec3 light = pow(max(0.f, dot(l, d)), 20.f) * vec3(1, 0.9, 0.8) * 15;

  return sky + light + low;
}

void main() {


  color = irradiance(gl_WorldRayDirectionNV);
}
