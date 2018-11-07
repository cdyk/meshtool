#version 460

layout(std140, binding = 0) uniform ObjBuf{
  mat4 MP;
  mat3 N;
  float scale;
} objBuf;

layout(location = 0) in vec3 inInstancePos;
layout(location = 1) in vec3 inInstanceNrm;

layout(location = 0) out vec4 color;

void main() {
	vec3 pos = inInstancePos + (gl_VertexIndex != 0 ? objBuf.scale : 0.0f) * inInstanceNrm;
	color = vec4(0.4, 0.4, 1, 1);
	gl_Position = objBuf.MP * vec4(pos, 1);
}
