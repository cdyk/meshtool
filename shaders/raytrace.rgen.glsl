#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "raytrace.common.glsl"

layout(location = 0) rayPayloadNVX Payload payload;

void main()
{
  vec2 u = vec2(gl_LaunchIDNVX.x, gl_LaunchIDNVX.y) / vec2(gl_LaunchSizeNVX.xy);

  vec2 c = vec2(2, -2)*u + vec2(-1, 1);

  vec4 oh = (sceneBuf.Pinv) * vec4(c, -1, 1);
  vec3 o = (1.f / oh.w)*oh.xyz;

  vec4 of = (sceneBuf.Pinv) * vec4(c, 1, 1);
  vec3 f = (1.f / of.w)*of.xyz;
  vec3 d = normalize(f - o);

  //o = vec3(0, 0, -2.0);
  //d = normalize(vec3(u-vec2(0.5), 1));

  uint state = gl_LaunchSizeNVX.x * gl_LaunchIDNVX.y + gl_LaunchIDNVX.x + sceneBuf.rndState;

  uint N = 20;

  vec3 color = vec3(0, 0, 0);
  for (uint i = 0; i < N; i++) {
    state = wangHash(state);

    payload.state = state;
    traceNVX(topLevel,
             gl_RayFlagsOpaqueNVX | gl_RayFlagsCullBackFacingTrianglesNVX,   // rayFlags
             ~0u,                    // cullMask
             0,                      // stbRecordOffset
             0,                      // stbRecordStride
             0,                      // missIndex
             o,                      // origin
             0.001f,                 // tMin
             d,                      // dir
             1000.f,                 // tMax
             0);                     // Payload
    color += (1.f / N)*payload.color;
  }

  if (sceneBuf.stationaryFrames != 0) {
    vec3 prevCol = imageLoad(imageIn, ivec2(gl_LaunchIDNVX.xy)).rgb;
    float M = sceneBuf.stationaryFrames;
    color = ((M - 1)*prevCol + color)/M;
  }
  imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(color, 0.0f));
}
