#version 400
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location=0) in vec4 color;
layout(location=0) out vec4 fragColor;

void main() {
  fragColor = color;
}
