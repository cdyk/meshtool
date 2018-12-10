#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_shader_8bit_storage : require
#extension GL_EXT_nonuniform_qualifier : require
//#extension GL_KHX_shader_explicit_arithmetic_types : require
//#extension GL_KHX_shader_explicit_arithmetic_types_int8 : require
#extension GL_GOOGLE_include_directive : require

#include "raytrace.common.glsl"

layout(location = 0) rayPayloadInNV Payload payloadIn;
layout(location = 1) rayPayloadNV Payload payload;
layout(location = 2) hitAttributeNV vec3 hitAttribute;

void main() {

  TriangleData data = triangles[nonuniformEXT(gl_InstanceID)].data[gl_PrimitiveID];

  float w0 = hitAttribute.x;
  float w1 = hitAttribute.y;
  float w2 = 1.f - w0 - w1;
  vec3 n = w2 * vec3(data.n0x, data.n0y, data.n0z) +
           w0 * vec3(data.n1x, data.n1y, data.n1z) +
           w1 * vec3(data.n2x, data.n2y, data.n2z);

  vec3 o = gl_WorldRayOriginNV + gl_WorldRayDirectionNV * gl_HitTNV;
  vec3 d = normalize(vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz));

  vec3 u, v, w;
  orthonormal(u, v, w, n);

  uint state = payloadIn.state;
  vec3 d_l = randomCosineDir(state);
  vec3 d_w = normalize(d_l.x*u + d_l.y*v + d_l.z*w);

  //d_w = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);

  payload.state = state;
  traceNV(topLevel,
           gl_RayFlagsOpaqueNV | gl_RayFlagsCullBackFacingTrianglesNV,   // rayFlags    gl_RayFlagsTerminateOnFirstHitNV;
           ~0u,                    // cullMask
           0,                      // stbRecordOffset
           0,                      // stbRecordStride
           0,                      // missIndex
           o,                      // origin
           0.001f,                 // tMin
           d_w,                    // dir
           1000.f,                 // tMax
           1);                     // Payload

  payloadIn.color = 0.7f*payload.color;

  //vec3 l = vec3(sceneBuf.lx, sceneBuf.ly, sceneBuf.lz);
  //colorIn =  0.3f*max(0.5f,dot(n,d))*color;// irradiance(n);
}