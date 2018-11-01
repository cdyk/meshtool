#version 460
#extension GL_NVX_raytracing : require

layout(binding = 0) uniform accelerationStructureNVX topLevel;
layout(binding = 1, rgba8) uniform image2D image;
layout(std140, binding = 2) uniform SceneBuf {
  mat4 Pinv;
  float lx, ly, lz;   // light at top right behind cam
  float ux, uy, uz;   // camera up
  uint rndState;
} sceneBuf;


struct Payload
{
  vec3 color;
  uint state;
};

layout(location = 0) rayPayloadNVX Payload payload;

uint xorShift(uint state)
{
  state ^= (state << 13);
  state ^= (state >> 17);
  state ^= (state << 5);
  return state;
}

uint wangHash(uint state)
{
  state = (state ^ 61) ^ (state >> 16);
  state *= 9;
  state = state ^ (state >> 4);
  state *= 0x27d4eb2d;
  state = state ^ (state >> 15);
  return state;
}

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

  uint N = 50;

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

  imageStore(image, ivec2(gl_LaunchIDNVX.xy), vec4(color, 0.0f));
}
