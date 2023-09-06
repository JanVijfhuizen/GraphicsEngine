#version 450

layout(location = 0) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D img;

void main() 
{
    vec4 color = texture(img, fragPos);
     if(color.a < .01f)
        discard;
    outColor = color;
}