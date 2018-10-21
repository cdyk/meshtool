#version 400
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout(std140, binding = 0) uniform ObjBuf{
  mat4 MP;
  mat3 N;
} objBuf;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inInstancePos;
layout(location = 3) in vec3 inInstanceTan;
layout(location = 4) in vec3 inInstanceBiNormal;
layout(location = 5) in vec3 inInstanceNormal;

layout(location = 0) out vec4 color;

void main() {
  float l = 0.02f;

  vec3 p = inInstancePos +
    l * inPos.x * inInstanceTan +
    l * inPos.y * inInstanceBiNormal +
    l * inPos.z * inInstanceNormal;

  color = vec4(inColor, 1);
  gl_Position = objBuf.MP * vec4(p, 1);
}
