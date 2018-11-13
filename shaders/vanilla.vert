#version 450
#pragma shader_stage(vertex)
#extension GL_EXT_shader_16bit_storage: require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_GOOGLE_include_directive : require

#include "ShaderStructs.glsl"

layout(std140, binding = 0) uniform ObjBuf
{
  mat4 MP;
  mat3 N;
};

layout(binding=1) readonly buffer Vertices
{
  Vertex vertices[];
};


layout(location = 0) out vec4 albedo;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec2 texCoord;

void main() {
  vec3 pos = vec3(vertices[gl_VertexIndex].px,
                  vertices[gl_VertexIndex].py,
                  vertices[gl_VertexIndex].pz);
  vec3 nrm = vec3((1.0/127.0)*int(vertices[gl_VertexIndex].nx) - 1.0,
                  (1.0/127.0)*int(vertices[gl_VertexIndex].ny) - 1.0,
                  (1.0/127.0)*int(vertices[gl_VertexIndex].nz) - 1.0);
  vec4 col = vec4((1.0/255.0)*int(vertices[gl_VertexIndex].r),
                  (1.0/255.0)*int(vertices[gl_VertexIndex].g),
                  (1.0/255.0)*int(vertices[gl_VertexIndex].b),
                  (1.0/255.0)*int(vertices[gl_VertexIndex].a));
  vec2 tex = vec2(vertices[gl_VertexIndex].tu,
                  vertices[gl_VertexIndex].tv);

  normal = normalize(N * nrm);
  albedo = col;
  texCoord = tex;
  gl_Position = MP * vec4(pos, 1);
}
