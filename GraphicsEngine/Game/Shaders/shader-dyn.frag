#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D diffuse;
layout(set = 0, binding = 1) uniform sampler2D normal;

vec3 lightPos = vec3(.5f);
vec3 lightColor = vec3(1.f);
vec3 ambient = vec3(.1f);

void main() 
{
    vec4 color = texture(diffuse, fragPos) * vec4(fragColor, 1);
    if(color.a < .01f)
        discard;

    vec4 normal = texture(normal, fragPos);
    vec3 norm = normalize(normal.xyz);
    vec3 lightDir = normalize(lightPos - vec3(fragPos, 0.0));
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec4 result = vec4(ambient + diffuse, 1.0) * color;
    outColor = result;
}