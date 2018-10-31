#version 460
#extension GL_NVX_raytracing : require

struct TriangleData
{
  float n0x, n0y, n0z;
  float n1x, n1y, n1z;
  float n2x, n2y, n2z;
  float r, g, b;
};


layout(binding = 0) uniform accelerationStructureNVX topLevel;
layout(binding = 1, rgba8) uniform image2D image;
layout(std140, binding = 2) uniform SceneBuf {
  mat4 Pinv;
} sceneBuf;
layout(std140, binding = 3) buffer TriangleDataBuffer {
  TriangleData data[];
} triangles[];

layout(location = 0) rayPayloadNVX vec3 color;

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


  imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(color.rg, triangles[0].data[0].r, 0.0f));
}
