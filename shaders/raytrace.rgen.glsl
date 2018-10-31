#version 460
#extension GL_NVX_raytracing : require

layout(binding = 0) uniform accelerationStructureNVX topLevel;
layout(binding = 1, rgba8) uniform image2D image;

#if 1

layout(location = 0) rayPayloadNVX float hitValue;

void main()
{
  const vec2 pixelCenter = vec2(gl_LaunchIDNVX.xy) + vec2(0.5);
  const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeNVX.xy);

  imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(inUV.x, inUV.y, 0.0, 0.0));
}

#else

layout(std140, binding = 2) uniform SceneBuf {
  mat4 Pinv;
} sceneBuf;

layout(location = 0) rayPayloadNVX vec3 color;

void main()
{
  vec2 u = (vec2(gl_LaunchIDNVX.xy) + vec2(0.5f)) / vec2(gl_LaunchSizeNVX.xy);

  vec4 oh = sceneBuf.Pinv * vec4(u, 0, 1);
  vec3 o = (1.f / oh.w)*oh.xyz;

  vec4 of = sceneBuf.Pinv * vec4(u, 1, 1);
  vec3 f = (1.f / of.w)*of.xyz;
  vec3 d = normalize(f - o);

  traceNVX(topLevel,
           gl_RayFlagsOpaqueNVX,   // rayFlags
           ~0u,                    // cullMask
           0,                      // stbRecordOffset
           0,                      // stbRecordStride
           0,                      // missIndex
           o,                      // origin
           0.001f,                 // tMin
           d,                      // dir
           1000.f,                 // tMax
           0);                     // Payload

  imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(color, 0.0f));
}
#endif