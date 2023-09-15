#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;
layout(location = 2) in vec2 wFragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D diffuse;
layout(set = 0, binding = 1) uniform sampler2D normal;

struct Light
{
    vec3 color;
    vec3 pos;
    float intensity;
    float fallOf;
};

layout(set=0, binding = 2) uniform LightInfo
{
    vec3 ambient;
    int count;
} lightInfo; 

layout(std140, set = 0, binding = 3) readonly buffer LightBuffer
{
    Light lights[];
} lightBuffer;

void main() 
{
    vec4 color = texture(diffuse, fragPos) * vec4(fragColor, 1);
    if(color.a < .01f)
        discard;

    vec4 normal = texture(normal, fragPos);
    vec3 norm = normalize(normal.xyz);

    vec3 lightMul = vec3(0);

    for(int i = 0; i < lightInfo.count; i++)
    {
        Light light = lightBuffer.lights[i];
        float dis = length(light.pos - vec3(wFragPos, 0.0));
        vec3 lightDir = normalize(light.pos - vec3(wFragPos, 0.0));
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * light.color;
        diffuse *= light.intensity;
        diffuse -= light.fallOf * dis;
        diffuse = max(vec3(0), diffuse);
        lightMul += diffuse;
    }

    vec4 result = vec4(lightInfo.ambient + lightMul, 1.0) * color;
    outColor = result;
}