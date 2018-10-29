#version 460 core
#extension GL_NVX_raytracing : require
layout(set = 0, binding = 0, rgba8) uniform image2D image;
layout(set = 0, binding = 1) uniform accelerationStructureNVX as;
layout(location = 0) rayPayloadNVX float payload;

void main()
{
  vec4 col = vec4(0, 0, 0, 1);

  vec3 origin = vec3(float(gl_LaunchIDNVX.x) / float(gl_LaunchSizeNVX.x), float(gl_LaunchIDNVX.y) / float(gl_LaunchSizeNVX.y), 1.0);
  vec3 dir = vec3(0.0, 0.0, -1.0);

  traceNVX(as, 0, 0xff, 0, 1, 0, origin, 0.0, dir, 1000.0, 0);

  col.y = payload;

  imageStore(image, ivec2(gl_LaunchIDNVX.xy), col);
}