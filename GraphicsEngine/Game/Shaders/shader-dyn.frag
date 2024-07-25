#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;
layout(location = 2) in vec2 wFragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D diffuse;
layout(set = 0, binding = 1) uniform sampler2D normal;

struct Light
{
    vec4 color;
    vec4 pos;
    float intensity;
    float fallOf;
    float size;
    float specularity;
};

layout(std140, set=0, binding = 2) uniform LightInfo
{
    vec3 ambient;
    int count;
} lightInfo; 

layout(std140, set = 0, binding = 3) readonly buffer LightBuffer
{
    Light lights[];
} lightBuffer;

vec3 CalculateNormal()
{
    vec3 cFragPos = vec3(fragPos, 0);
    cFragPos.x *= 2;
    while(cFragPos.x > 1.0)
        cFragPos.x -= 1.0;
    cFragPos -= vec3(.5, .5, 0);
    return cFragPos;
}

void main() 
{
    vec4 color = texture(diffuse, fragPos) * vec4(fragColor, 1);
    if(color.a < .01f)
        discard;

    vec4 n = texture(normal, fragPos);
    vec3 norm = n.xyz; //(CalculateNormal() + n.xyz);

    vec3 lightMul = vec3(0);

    for(int i = 0; i < lightInfo.count; i++)
    {
        Light light = lightBuffer.lights[i];
        float dis = length(light.pos.xyz - vec3(wFragPos, 0.0)) - light.size;
        dis = max(0.0, dis);
        vec3 lightDir = normalize(light.pos.xyz - vec3(wFragPos, 0.0));
        float normAngle = dot(norm, lightDir);
        float diff = max(normAngle, 0.0);
        vec3 diffuse = diff * light.color.xyz;
        diffuse *= light.intensity;
        diffuse -= light.fallOf * dis;
        diffuse = max(vec3(0), diffuse);

        // Specularity.
        vec3 viewDir = vec3(0, 0, -1);
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        vec3 specular = light.color.xyz * light.specularity * spec;  
        diffuse += specular;

        lightMul += diffuse;
    }

    vec4 result = vec4(lightInfo.ambient + lightMul, 1.0) * color;
    outColor = result;
}