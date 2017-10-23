#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConsts
{
  mat4 model;
} pushConsts;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 outTexCoord;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main() {
    gl_Position = ubo.proj * ubo.view * 
    	pushConsts.model * vec4(position, 1.0);
    outTexCoord = texcoord;
}