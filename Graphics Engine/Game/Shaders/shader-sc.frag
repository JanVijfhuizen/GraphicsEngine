#version 450

layout(location = 0) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D img;

float rand(vec2 n) { 
	return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p){
	vec2 ip = floor(p);
	vec2 u = fract(p);
	u = u*u*(3.0-2.0*u);
	
	float res = mix(
		mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
		mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
	return res*res;
}

layout(push_constant) uniform PushConstants
{
    ivec2 resolution;
    ivec2 simResolution;
    float time;
} pushConstants;

void main() 
{
    float m =  pushConstants.resolution.x / pushConstants.resolution.y;
    vec2 res = vec2(pushConstants.simResolution.x * m, pushConstants.simResolution.y);

    vec2 uv = fragPos;
    uv *= res;
    uv = floor(uv);
    uv /= res;
    
    float nx = noise(uv.xy * 100.0);
    float ny = noise(uv.yx * 100.0);
    
    float v = sin(nx * pushConstants.time + ny * pushConstants.time);
    v *= .05f;

    vec4 color = texture(img, fragPos);
    outColor = color.a < .01f ? vec4(vec3(v), 1.0) : color;
}