#version 450

layout(location = 0) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D img;

layout(push_constant) uniform PushConstants
{
    vec2 resolution;
    float time;
} pushConstants;

void main() 
{
    vec4 color = texture(img, fragPos);
    color.r *= sin(pushConstants.time);
     if(color.a < .01f)
        discard;
    outColor = color;
}