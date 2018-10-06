#version 400
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout(std140, binding = 0) uniform ObjBuf{
  mat4 MP;
  mat3 N;
} objBuf;

layout(location = 0) in vec3 pos;
layout(location = 0) out vec4 color;

void main() {
  color = vec4(1, 1, 1, 0);
  gl_Position = objBuf.MP * vec4(pos, 1);
}