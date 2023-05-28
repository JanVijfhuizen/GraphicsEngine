#version 450

layout(location = 0) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textureAtlas;

void main() 
{
    outColor = texture(textureAtlas, fragPos);
}