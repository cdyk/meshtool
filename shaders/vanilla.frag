#version 450
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec4 albedo;
layout(location = 1) in vec3 normal;
layout(location = 0) out vec4 fragColor;

void main() {
  float diffuse = max(0.2, dot(normalize(normal),
                               normalize(vec3(1, 1, 1))));

  fragColor = vec4(diffuse * albedo.rgb, albedo.a);
}
