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

float applyVignette(vec2 uv)
{
    uv *=  1.0 - uv.yx;
    float vig = uv.x*uv.y * 30.0;
    vig = pow(vig, 0.25);
    return vig;
}

vec2 Dist(vec2 pos, vec2 res)
{
	pos = pos * res;
	pos -= floor(pos);
	pos -= vec2(.5);
	pos = abs(pos);
	pos.xy = vec2(max(pos.x, pos.y));
	return pos;
}

void main() 
{
    float m =  pushConstants.resolution.x / pushConstants.resolution.y;
    vec2 res = vec2(pushConstants.simResolution.x * m, pushConstants.simResolution.y);

    vec2 uv = fragPos;
    
    vec2 d = Dist(uv, res);
    float b = length(d);
    b = step(.45, b);

    uv *= res;
    uv = floor(uv);
    uv /= res;
    
    float nx = noise(uv.xy * pushConstants.simResolution.x);
    float ny = noise(uv.yx * pushConstants.simResolution.y);
    
    float v = sin(nx * pushConstants.time + ny * pushConstants.time);
    v *= .02f;
    v *= applyVignette(uv);

    // Light.
    vec2 center = vec2(.2, .2);
    float dist = 1.0/length(uv - center);
    //v *= dist * 3.4f;
    //v = abs(v);

    vec4 color = texture(img, fragPos);
    vec4 bgColor = vec4(vec3(v), 1.0);
    outColor = mix(color, bgColor, 1.f - color.a) - vec4(vec3(b), 0.0);
}