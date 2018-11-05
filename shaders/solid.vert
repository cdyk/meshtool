#version 450
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout(std140, binding = 0) uniform ObjBuf{
  mat4 MP;
  mat3 N;
} objBuf;


layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 nrm;
layout(location = 2) in vec2 tex;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec4 albedo;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 texCoord;


void main() {
  normal = normalize(objBuf.N * nrm);
  albedo = inColor;
  texCoord = tex;
  gl_Position = objBuf.MP * vec4(pos, 1);
}
