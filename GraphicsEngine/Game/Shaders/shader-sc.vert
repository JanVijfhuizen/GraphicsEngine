#version 450
#include "utils.shader"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

layout(location = 0) out vec2 fragPos;

void main() 
{
   gl_Position = vec4(inPosition, 1);
   fragPos = inTexCoords;
}